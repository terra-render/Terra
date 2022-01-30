// Header
#include <Renderer.hpp>

// C++ STL
#include <thread>
#include <algorithm>

// Satellite
#include <Logging.hpp>
#define CLOTO_IMPLEMENTATION
#include <Cloto.h>
#include <Config.hpp>

// Terra
#include <TerraProfile.h>
#include <TerraPresets.h>
namespace {
    // fnv1a
    constexpr uint64_t fnv_basis = 14695981039346656037ull;
    constexpr uint64_t fnv_prime = 1099511628211ull;
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
    static_assert ( sizeof ( TileMsgArg ) <= CLOTO_MSG_PAYLOAD_SIZE, "TileMsgArg size is too big to be inlined, reduce it or use CLOTO_MSG_JOB_REMOTE_ARGS." );
    using Args = TerraRenderer::TerraRenderArgs;
    Args* args = ( Args* ) _args;

    if ( args->renderer->_on_tile_begin ) {
        ClotoMessageJobPayload msg;
        msg.routine = tile_msg_stub;
        TileMsgArg msg_arg { args->renderer->_on_tile_begin, args->x, args->y, args->width, args->height };
        memcpy ( msg.buffer, &msg_arg, sizeof ( msg_arg ) );
        cloto_thread_send_message ( args->renderer->thread(), CLOTO_MSG_JOB_LOCAL_ARGS, &msg, sizeof ( msg ) );
    }

    terra_render ( args->renderer->_target_camera, args->renderer->_target_scene, &args->renderer->_framebuffer, args->x, args->y, args->width, args->height );
    TERRA_PROFILE_UPDATE_LOCAL_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER );
    TERRA_PROFILE_UPDATE_LOCAL_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY );
    TERRA_PROFILE_UPDATE_LOCAL_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE );
    TERRA_PROFILE_UPDATE_LOCAL_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION );

    if ( args->renderer->_on_tile_end ) {
        ClotoMessageJobPayload msg;
        msg.routine = tile_msg_stub;
        TileMsgArg msg_arg { args->renderer->_on_tile_end, args->x, args->y, args->width, args->height };
        memcpy ( msg.buffer, &msg_arg, sizeof ( msg_arg ) );
        cloto_thread_send_message ( args->renderer->thread(), CLOTO_MSG_JOB_LOCAL_ARGS, &msg, sizeof ( msg ) );
    }

    cloto_atomic_fetch_add_u32 ( &args->renderer->_tile_counter, -1 );
}

TerraRenderer::TerraRenderer ( ) {
    cloto_thread_register();
    _this_thread = cloto_thread_get();
    memset ( &_framebuffer, 0, sizeof ( TerraFramebuffer ) );
    _paused = true;
    _tile_counter = 0;
    _target_camera = nullptr;
    _target_scene = nullptr;
}

TerraRenderer::~TerraRenderer() {
    if ( _workers != nullptr ) {
        cloto_slavegroup_destroy ( _workers.get() );
    }

    cloto_thread_dispose();
}

void TerraRenderer::update() {
    _process_messages();

    if ( _paused ) {
        return;
    }

    // The current job queue has finished executing, we can now read from TerraFramebuffer
    if ( _tile_counter == 0 ) {
        _update_stats();

        // Notifying
        if ( _on_step_end ) {
            _on_step_end();
        }

        if ( _iterative ) {
            Log::verbose ( FMT ( "Finished iteration %d", _iterations ) );

            if ( _opt_render_change ) {
                // TODO move this out, make the rendering stop asap
                Log::warning ( STR ( "Rendering settings were changed. Call step() or loop() to begin a new rendering." ) );
                _paused = true;
            } else {
                if ( _opt_job_change ) {
                    _setup_threads();
                    _opt_job_change = false;
                    _push_jobs();
                } else {
                    _restart_jobs();
                }

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
    _clear_stats();
    _clear_framebuffer = true;
}

bool TerraRenderer::step ( const TerraCamera& camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end ) {
    if ( !_paused ) {
        Log::error ( STR ( "Rendering is already in progress." ) );
        return false;
    }

    _target_scene   = scene;
    _target_camera  = &camera;
    _on_step_end    = on_step_end;
    _on_tile_begin  = on_tile_begin;
    _on_tile_end    = on_tile_end;
    _iterative      = false;
    return _launch ();
}

bool TerraRenderer::loop ( const TerraCamera& camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end ) {
    if ( !_paused ) {
        Log::error ( STR ( "Rendering is already in progress." ) );
        return false;
    }

    _target_scene   = scene;
    _target_camera  = &camera;
    _on_step_end    = on_step_end;
    _on_tile_begin  = on_tile_begin;
    _on_tile_end    = on_tile_end;
    _iterative      = true;
    return _launch ();
}

void TerraRenderer::pause() {
    if ( !_paused ) {
        Log::verbose ( STR ( "Renderer will pause at the end of current step" ) );
        _paused = true;
    } else {
        Log::verbose ( STR ( "Renderer is already paused" ) );
    }
}

void TerraRenderer::update_config() {
    // We defer the actual update until a new rendering step is required
    if ( _tile_size != Config::read_i ( Config::JOB_TILE_SIZE )
            || _worker_count != Config::read_f ( Config::JOB_N_WORKERS )
       ) {
        // Update the job system, keep the current rendering valid
        _opt_job_change = true;
    }

    if ( _width != Config::read_i ( Config::RENDER_WIDTH )
            || _height != Config::read_i ( Config::RENDER_HEIGHT )
       ) {
        // Restart the rendering
        _opt_render_change = true;
    }
}

const TextureData& TerraRenderer::framebuffer() {
    assert ( sizeof ( TerraFloat3 ) == sizeof ( float ) * 3 );
    _framebuffer_data.data = ( float* ) _framebuffer.pixels;
    _framebuffer_data.width = ( int ) _framebuffer.width;
    _framebuffer_data.height = ( int ) _framebuffer.height;
    _framebuffer_data.components = 3;
    return _framebuffer_data;
}

bool TerraRenderer::is_framebuffer_clear() const {
    return _clear_framebuffer;
}

int TerraRenderer::iterations() const {
    return _iterations;
}

ClotoThread* TerraRenderer::thread() const {
    return _this_thread;
}

void TerraRenderer::_update_stats() {
    TERRA_PROFILE_UPDATE_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER );
    TERRA_PROFILE_UPDATE_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE );
    TERRA_PROFILE_UPDATE_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY );
    TERRA_PROFILE_UPDATE_STATS ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION );
}

void TerraRenderer::_clear_stats() {
    TERRA_PROFILE_CLEAR_TARGET ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER );
    TERRA_PROFILE_CLEAR_TARGET ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE );
    TERRA_PROFILE_CLEAR_TARGET ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY );
    TERRA_PROFILE_CLEAR_TARGET ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION );
}

void TerraRenderer::_num_tiles ( int& tiles_x, int& tiles_y ) {
    int tile_size = Config::read_i ( Config::Opts::JOB_TILE_SIZE );

    if ( _framebuffer.pixels == nullptr || tile_size < 0 ) {
        Log::error ( STR ( "Internal state not initialized" ) );
        return;
    }

    tiles_x = ( int ) ceilf ( ( float ) _framebuffer.width / tile_size );
    tiles_y = ( int ) ceilf ( ( float ) _framebuffer.height / tile_size );
}

void TerraRenderer::_setup_threads () {
    int workers = Config::read_i ( Config::JOB_N_WORKERS );

    // Free previous allocations
    if ( _workers != nullptr ) {
        cloto_slavegroup_destroy ( _workers.get() );
        _workers = nullptr;
        TERRA_PROFILE_DELETE_SESSION ( TERRA_PROFILE_SESSION_DEFAULT );
    }

    // Compute tile/queue size
    int tx, ty;
    _num_tiles ( tx, ty );
    int job_buffer_size = tx * ty;
    // Rounding to next power of two, we should also try the `countleadingzeros` intrinsic
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
    // create workers
    _workers.reset ( new ClotoSlaveGroup );
    cloto_slavegroup_create ( _workers.get(), workers, job_buffer_size );
    // setup profiler
    TERRA_PROFILE_CREATE_SESSION ( TERRA_PROFILE_SESSION_DEFAULT, workers );
    TERRA_PROFILE_CREATE_TARGET ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER, 0xaffff );
    TERRA_PROFILE_CREATE_TARGET ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE, 0xaffff );
    TERRA_PROFILE_CREATE_TARGET ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY, 0xaffff );
    TERRA_PROFILE_CREATE_TARGET ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, 0xafffff );

    for ( int i = 0; i < workers; ++i ) {
        ClotoMessageJobPayload payload;
        auto msg_routine = [] ( void* args ) -> void {
            size_t* session = ( size_t* ) args;
            TERRA_PROFILE_REGISTER_THREAD ( *session );
        };
        payload.routine = msg_routine;
        size_t session = TERRA_PROFILE_SESSION_DEFAULT;
        memcpy ( payload.buffer, &session, sizeof ( session ) );
        cloto_thread_send_message ( &_workers->slaves[i].thread, CLOTO_MSG_JOB_LOCAL_ARGS, &payload, CLOTO_MSG_PAYLOAD_SIZE );
    }

    // create jobs
    int tile_size = Config::read_i ( Config::Opts::JOB_TILE_SIZE );
    int num_tiles_x, num_tiles_y;
    _num_tiles ( num_tiles_x, num_tiles_y );
    _job_args.clear();
    _job_args.resize ( num_tiles_x * num_tiles_y );

    for ( int i = 0; i < num_tiles_y; ++i ) {
        for ( int j = 0; j < num_tiles_x; ++j ) {
            TerraRenderArgs* args = _job_args.data() + i * num_tiles_x + j;
            args->renderer = this;
            args->x = j * tile_size;
            args->y = i * tile_size;
            args->width = ( int ) terra_mini ( ( size_t ) ( j + 1 ) * tile_size, _framebuffer.width ) - j * tile_size;
            args->height = ( int ) terra_mini ( ( size_t ) ( i + 1 ) * tile_size, _framebuffer.height ) - i * tile_size;
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
            TerraRenderArgs* args = _job_args.data() + i * num_tiles_x + j;
            ClotoJob job;
            job.routine = &terra_render_launcher;
            job.args = args;
            cloto_workqueue_push ( &_workers->queue, &job );
        }
    }
}

void TerraRenderer::_restart_jobs() {
    int tx, ty;
    _num_tiles ( tx, ty );
    int n_jobs = tx * ty;
    _tile_counter = n_jobs;
    cloto_slavegroup_reset ( _workers.get() );
}

bool TerraRenderer::_launch () {
    //if ( _framebuffer.pixels == nullptr ) {
    //    Log::error ( STR ( "Cannot launch rendering, invalid destination Terra framebuffer" ) );
    //    return false;
    //}
    assert ( _target_camera );
    assert ( _target_scene );
    assert ( _paused );

    // Sync config
    if ( _opt_render_change ) {
        if ( _framebuffer.pixels != nullptr ) {
            terra_framebuffer_destroy ( &_framebuffer );
            memset ( &_framebuffer, 0, sizeof ( TerraFramebuffer ) );
        }

        int width = Config::read_i ( Config::Opts::RENDER_WIDTH );
        int height = Config::read_i ( Config::Opts::RENDER_HEIGHT );

        if ( !terra_framebuffer_create ( &_framebuffer, width, height ) ) {
            Log::error ( STR ( "Failed to create Terra framebuffer, subsequent calls will fail" ) );
            return false;
        }

        Log::verbose ( FMT ( "Creating Terra framebuffer %dx%d", _framebuffer.width, _framebuffer.height ) );
        Log::verbose ( STR ( "Creating Terra render jobs" ) );
        // Creating workers and jobs (required on framebuffer size change)
        _setup_threads ();
        // Update camera
    } else if ( _opt_job_change ) {
        _setup_threads ();
    }

    _opt_job_change = false;
    _opt_render_change = false;
    // Setup internal state
    _paused             = false;
    _iterations         = 0;
    _clear_framebuffer  = false;
    // Push jobs
    _push_jobs();
    return true;
}

void TerraRenderer::_process_messages() {
    cloto_thread_process_messages ( _this_thread );
}
