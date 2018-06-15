// Header
#include <Renderer.hpp>

// C++ STL
#include <thread>
#include <algorithm>

// Satellite
#include <Logging.hpp>
#define CLOTO_IMPLEMENTATION
#include <Cloto.h>

namespace {
    // fnv1a - Generic unsafe hash, I would really be surprised if this resulted in collisions
    constexpr uint64_t fnv_basis = 14695981039346656037ull;
    constexpr uint64_t fnv_prime = 1099511628211ull;

    // FNV-1a 64 bit hash of null terminated buffer
    uint64_t fnv1a_hash ( const char* str, uint64_t hash = fnv_basis ) {
        return *str ? fnv1a_hash ( str + 1, ( hash ^ *str ) * fnv_prime ) : hash;
    }
}

namespace std {
    template <>
    struct hash<HTerraScene> {
        uint64_t operator() ( const HTerraScene& k ) const {
            constexpr size_t buf_size = sizeof ( HTerraScene );
            char buf[buf_size + 1];
            memcpy ( buf, &k, buf_size );
            buf[buf_size] = '\0';
            return fnv1a_hash ( buf );
        }
    };

    template <>
    struct hash<TerraCamera> {
        uint64_t operator() ( const TerraCamera& k ) const {
            constexpr size_t buf_size = sizeof ( TerraCamera );
            char buf[buf_size + 1];
            memcpy ( buf, &k, buf_size );
            buf[buf_size] = '\0';
            return fnv1a_hash ( buf );
        }
    };
}

using namespace std;

typedef struct {
    TerraRenderer::TileEvent& event;
    size_t x;
    size_t y;
    size_t w;
    size_t h;
} TileMsgArg;

typedef struct {
    ClotoJobRoutine* routine;
    TileMsgArg arg;
} TileMsg;

void tile_msg_stub ( void* _arg ) {
    TileMsgArg* arg = ( TileMsgArg* ) _arg;
    arg->event ( arg->x, arg->y, arg->w, arg->h );
}

void terra_render_launcher ( void* _args ) {
    using Args = TerraRenderer::TerraRenderArgs;
    Args* args = ( Args* ) _args;

    if ( args->th->_on_tile_begin ) {
        TileMsg msg1{ tile_msg_stub, args->th->_on_tile_begin, args->x, args->y, args->width, args->height };
        cloto_thread_send_message ( args->th->thread(), &msg1, sizeof ( msg1 ), CLOTO_MSG_JOB_LOCAL_ARGS );
    }

    terra_render ( args->th->_target_camera, args->th->_target_scene, &args->th->_framebuffer, args->x, args->y, args->width, args->height );

    if ( args->th->_on_tile_end ) {
        TileMsg msg2{ tile_msg_stub, args->th->_on_tile_end, args->x, args->y, args->width, args->height };
        cloto_thread_send_message ( args->th->thread(), &msg2, sizeof ( msg2 ), CLOTO_MSG_JOB_LOCAL_ARGS );
    }

    cloto_atomic_fetch_add_u32 ( &args->th->_tile_counter, -1 );
}

TerraRenderer::TerraRenderer ( ) {
    cloto_thread_register();
    _this_thread = cloto_thread_get();
}

TerraRenderer::~TerraRenderer() {
    if ( _workers != nullptr ) {
        cloto_slavegroup_destroy ( _workers.get() );
    }

    cloto_thread_dispose();
}

bool TerraRenderer::init ( int width, int height, int tile_side_length, int concurrent_jobs ) {
    if ( concurrent_jobs == -1 ) {
        concurrent_jobs = std::thread::hardware_concurrency();
    }

    if ( concurrent_jobs == 0 ) {
        concurrent_jobs = 1;
    }

    // Initial state
    memset ( &_framebuffer, 0, sizeof ( TerraFramebuffer ) );
    _paused          = true;
    _iterative       = true;
    _progressive     = true;
    _tile_counter    = 0;
    _tile_size       = 0; // Triggers job creation when _apply_changes is called()
    _concurrent_jobs = 0;
    _target_camera   = nullptr;
    _target_scene    = nullptr;
    // Initialization values
    _scene_id             = 0x0;
    _next_tile_size       = tile_side_length;
    _next_concurrent_jobs = concurrent_jobs;
    _next_width           = width;
    _next_height          = height;
    return _apply_changes();
}

void TerraRenderer::resize ( int width, int height ) {
    _next_width = width;
    _next_height = height;

    if ( is_rendering() ) {
        Log::warning ( STR ( "Can't resize while rendering. Changes will be applied at the end of the step." ) );
        return;
    }
}

void TerraRenderer::set_concurrent_jobs ( int concurrent_jobs ) {
    _next_concurrent_jobs = concurrent_jobs;

    if ( is_rendering() ) {
        Log::warning ( STR ( "Can't resize while rendering. Changes will be applied at the end of the step." ) );
        return;
    }
}

void TerraRenderer::set_tile_size ( int tile_side_length ) {
    _next_tile_size = tile_side_length;

    if ( is_rendering() ) {
        Log::warning ( STR ( "Can't resize while rendering. Changes will be applied at the end of the step." ) );
        return;
    }
}

void TerraRenderer::set_progressive ( bool progressive ) {
    _progressive = progressive;
}

void TerraRenderer::refresh_jobs() {
    _process_messages();

    if ( is_paused() ) {
        return;
    }

    // The current job queue has finished executing, we can now read from TerraFramebuffer
    if ( _tile_counter == 0 ) {
        // Notifying
        if ( _on_step_end ) {
            _on_step_end();
        }

        if ( is_iterative() ) {
            Log::verbose ( FMT ( "Finished iteration %d", _iterations ) );

            // Let's give time to save the file / ..
            if ( _state_changed() ) {
                Log::warning ( STR ( "Renderer settings changed while looping, Renderer is now paused. Call step() or loop() to resume." ) );
                _paused = true;
            } else {
                int tx, ty;
                _num_tiles ( tx, ty );
                int n_jobs = tx * ty;
                _tile_counter = n_jobs;
                cloto_slavegroup_reset ( _workers.get() );
                ++_iterations;
            }
        } else {
            Log::verbose ( STR ( "Finished step" ) );
            _paused = true;
        }
    }
}

void TerraRenderer::clear() {
    terra_framebuffer_clear ( &_framebuffer );
}

bool TerraRenderer::step ( TerraCamera* camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end ) {
    if ( is_rendering() ) {
        Log::error ( STR ( "Cannot launch rendering while another one is in progress." ) );
        return false;
    }

    _target_scene  = scene;
    _target_camera = camera;
    _on_step_end   = on_step_end;
    _on_tile_begin = on_tile_begin;
    _on_tile_end   = on_tile_end;
    _iterative     = false;
    return _launch ();
}

bool TerraRenderer::loop ( TerraCamera* camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end ) {
    if ( is_rendering() ) {
        Log::error ( STR ( "Cannot launch rendering while another one is in progress." ) );
        return false;
    }

    _target_scene  = scene;
    _target_camera = camera;
    _on_step_end   = on_step_end;
    _on_tile_begin = on_tile_begin;
    _on_tile_end   = on_tile_end;
    _iterative     = true;
    return _launch ();
}

void TerraRenderer::pause_at_next_step() {
    Log::verbose ( STR ( "Pausing rendering at the end of the current step" ) );
    _iterative = false;
}

const TextureData& TerraRenderer::framebuffer() {
    assert ( sizeof ( TerraFloat3 ) == sizeof ( float ) * 3 );
    _framebuffer_data.data = ( float* ) _framebuffer.pixels;
    _framebuffer_data.width = ( int ) _framebuffer.width;
    _framebuffer_data.height = ( int ) _framebuffer.height;
    _framebuffer_data.components = 3;
    return _framebuffer_data;
}

const TerraSceneOptions& TerraRenderer::options() const {
    return _options;
}

bool TerraRenderer::is_rendering() const {
    return !_paused;
}

bool TerraRenderer::is_paused() const {
    return _paused;
}

bool TerraRenderer::is_iterative() const {
    return _iterative;
}

bool TerraRenderer::is_progressive() const {
    return _progressive;
}

int TerraRenderer::iterations() const {
    return _iterations;
}

int TerraRenderer::concurrent_jobs() const {
    return _concurrent_jobs;
}

int TerraRenderer::tile_size() const {
    return _tile_size;
}

ClotoThread* TerraRenderer::thread() const {
    return _this_thread;
}

void TerraRenderer::_create_jobs() {
    _job_args.clear();
    const int num_tiles_x = ( int ) ceilf ( ( float ) _framebuffer.width / _tile_size );
    const int num_tiles_y = ( int ) ceilf ( ( float ) _framebuffer.height / _tile_size );
    _job_args.resize ( num_tiles_x * num_tiles_y );

    for ( int i = 0; i < num_tiles_y; ++i ) {
        for ( int j = 0; j < num_tiles_x; ++j ) {
            TerraRenderArgs* args = _job_args.data() + i * num_tiles_x + j;
            args->th = this;
            args->x = j * _tile_size;
            args->y = i * _tile_size;
            args->width =  ( int ) terra_mini ( ( size_t ) ( j + 1 ) * _tile_size,  _framebuffer.width ) - j * _tile_size;
            args->height = ( int ) terra_mini ( ( size_t ) ( i + 1 ) * _tile_size, _framebuffer.height ) - i * _tile_size;
        }
    }
}

void TerraRenderer::_push_jobs() {
    int num_tiles_x, num_tiles_y;
    _num_tiles ( num_tiles_x, num_tiles_y );
    _tile_counter = num_tiles_x * num_tiles_y;
    Log::verbose ( FMT ( "Pushing %d jobs", _tile_counter ) );
    cloto_workqueue_clear ( &_workers->queue );

    for ( int i = 0; i < num_tiles_y; ++i ) {
        for ( int j = 0; j < num_tiles_x; ++j ) {
            TerraRenderArgs* args = _job_args.data() +  i * num_tiles_x + j;
            ClotoJob job;
            job.routine = &terra_render_launcher;
            job.args = args;
            cloto_workqueue_push ( &_workers->queue, &job );
        }
    }
}

void TerraRenderer::_num_tiles ( int& tiles_x, int& tiles_y ) {
    if ( _framebuffer.pixels == nullptr || _tile_size < 0 ) {
        Log::error ( STR ( "Internal state not initialized" ) );
        return;
    }

    tiles_x = ( int ) ceilf ( ( float ) _framebuffer.width / _tile_size );
    tiles_y = ( int ) ceilf ( ( float ) _framebuffer.height / _tile_size );
}

uint64_t TerraRenderer::_gen_scene_id ( const TerraCamera* camera, HTerraScene scene ) {
    return hash<HTerraScene> () ( scene ) ^ hash<TerraCamera>() ( *camera );
    //return hash<uint64_t>() ( ( uint64_t& ) camera ) ^ hash<uint64_t>() ( ( uint64_t& ) scene );
}

bool TerraRenderer::_state_changed() {
    return _framebuffer.width != _next_width ||
           _framebuffer.height != _next_height ||
           _tile_size != _next_tile_size ||
           _concurrent_jobs != _next_concurrent_jobs;
}

bool TerraRenderer::_apply_changes() {
    bool threading_changed = _tile_size != _next_tile_size || _concurrent_jobs != _next_concurrent_jobs;
    bool resolution_changed = _next_width != _framebuffer.width || _next_height != _framebuffer.height;

    if ( resolution_changed ) {
        if ( _framebuffer.pixels != nullptr ) {
            terra_framebuffer_destroy ( &_framebuffer );
            memset ( &_framebuffer, 0, sizeof ( TerraFramebuffer ) );
        }

        if ( !terra_framebuffer_create ( &_framebuffer, _next_width, _next_height ) ) {
            Log::error ( STR ( "Failed to create Terra framebuffer, subsequent calls will fail" ) );
            return false;
        }

        Log::verbose ( FMT ( "Creating Terra framebuffer %dx%d", _framebuffer.width, _framebuffer.height ) );
        _next_width  = ( int ) _framebuffer.width;
        _next_height = ( int ) _framebuffer.height;
    }

    if ( threading_changed ) {
        Log::verbose ( STR ( "Creating Terra render jobs" ) );

        if ( _workers != nullptr ) {
            cloto_slavegroup_destroy ( _workers.get() );
            _workers = nullptr;
        }

        // Initializing state
        _tile_size = _next_tile_size;
        _concurrent_jobs = _next_concurrent_jobs;
        // Creating workers and jobs
        int tx, ty;
        _num_tiles ( tx, ty );
        int job_buffer_size = tx * ty;
        // Rounding to next power of two, can also be done using `countleadingzeros` intrinsic
        // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        {
            --job_buffer_size;
            job_buffer_size |= job_buffer_size >> 1;
            job_buffer_size |= job_buffer_size >> 2;
            job_buffer_size |= job_buffer_size >> 4;
            job_buffer_size |= job_buffer_size >> 8;
            job_buffer_size |= job_buffer_size >> 16;
            ++job_buffer_size;
        }
        _workers.reset (  new ClotoSlaveGroup );
        cloto_slavegroup_create ( _workers.get(), _concurrent_jobs, job_buffer_size );
        _create_jobs();
    }

    return true;
}

bool TerraRenderer::_launch () {
    if ( _target_camera == nullptr ||  _target_scene == nullptr ) {
        return false;
    }

    if ( _framebuffer.pixels == nullptr ) {
        Log::error ( STR ( "Cannot launch rendering, invalid destination Terra framebuffer" ) );
        return false;
    }

    assert ( is_paused() );
    // Updating options
    _apply_changes();
    // Clearing the framebuffer if the scene has changes
    uint64_t scene_id = _gen_scene_id ( _target_camera, _target_scene );

    if ( _scene_id != scene_id ) {
        terra_framebuffer_clear ( &_framebuffer );
        _scene_id = scene_id;
    }

    // Setting internal state (used only by TerraRenderer)
    _paused      = false;
    _iterations  = 0;
    _options = *terra_scene_get_options ( _target_scene );
    // Pushing jobs
    _push_jobs();
    return true;
}

void TerraRenderer::_process_messages() {
    cloto_thread_process_messages ( _this_thread );
}