// Header
#include <Renderers/TerraRenderer.hpp>

// C++ STL
#include <thread>
#include <algorithm>

// Satellite
#include <Logging.hpp>
#define CLOTO_IMPLEMENTATION
#include <Cloto.h>
#include <Scene.hpp>
#include <Config.hpp>
#include <Camera.hpp>

// Terra
#include <TerraProfile.h>
#include <TerraPresets.h>

// todo: remove
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {
    // fnv1a
    constexpr uint64_t fnv_basis = 14695981039346656037ull;
    constexpr uint64_t fnv_prime = 1099511628211ull;
    uint64_t fnv1a_hash(const char* str, uint64_t hash = fnv_basis) {
        return *str ? fnv1a_hash(str + 1, (hash ^ *str) * fnv_prime) : hash;
    }
}

namespace std {
    template <>
    struct hash<HTerraScene> {
        uint64_t operator() (const HTerraScene& k) const {
            constexpr size_t buf_size = sizeof(HTerraScene);
            char buf[buf_size + 1];
            memcpy(buf, &k, buf_size);
            buf[buf_size] = '\0';
            return fnv1a_hash(buf);
        }
    };

    template <>
    struct hash<TerraCamera> {
        uint64_t operator() (const TerraCamera& k) const {
            constexpr size_t buf_size = sizeof(TerraCamera);
            char buf[buf_size + 1];
            memcpy(buf, &k, buf_size);
            buf[buf_size] = '\0';
            return fnv1a_hash(buf);
        }
    };
}

using namespace std;

typedef struct {
    Renderer::TileEvent& event;
    size_t x;
    size_t y;
    size_t w;
    size_t h;
} TileMsgArg;

typedef struct {
    ClotoJobRoutine* routine;
    TileMsgArg arg;
} TileMsg;

void tile_msg_stub(void* _arg) {
    TileMsgArg* arg = (TileMsgArg*)_arg;
    arg->event(arg->x, arg->y, arg->w, arg->h);
}

void terra_render_launcher(void* _args) {
    using Args = TerraRenderer::TerraRenderArgs;
    Args* args = (Args*)_args;

    if (args->th->_on_tile_begin) {
        //TileMsg msg1{ tile_msg_stub, args->th->_on_tile_begin, args->x, args->y, args->width, args->height };
        ClotoMessageJobPayload msg;
        msg.routine = tile_msg_stub;
        TileMsgArg msg_arg{ args->th->_on_tile_begin, args->x, args->y, args->width, args->height };
        memcpy(msg.buffer, &msg_arg, sizeof(msg_arg));
        cloto_thread_send_message(args->th->thread(), CLOTO_MSG_JOB_LOCAL_ARGS, &msg, sizeof(msg));
    }

    terra_render(args->th->_target_camera, args->th->_target_scene, &args->th->_framebuffer, args->x, args->y, args->width, args->height);
    TERRA_PROFILE_UPDATE_LOCAL_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER);
    TERRA_PROFILE_UPDATE_LOCAL_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY);
    TERRA_PROFILE_UPDATE_LOCAL_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE);
    TERRA_PROFILE_UPDATE_LOCAL_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION);

    if (args->th->_on_tile_end) {
        TileMsg msg2{ tile_msg_stub, args->th->_on_tile_end, args->x, args->y, args->width, args->height };
        ClotoMessageJobPayload msg;
        msg.routine = tile_msg_stub;
        TileMsgArg msg_arg{ args->th->_on_tile_end, args->x, args->y, args->width, args->height };
        memcpy(msg.buffer, &msg_arg, sizeof(msg_arg));
        cloto_thread_send_message(args->th->thread(), CLOTO_MSG_JOB_LOCAL_ARGS, &msg, sizeof(msg));
    }

    cloto_atomic_fetch_add_u32(&args->th->_tile_counter, -1);
}

TerraRenderer::TerraRenderer() :
    Renderer(),
    _bvh(nullptr) {

    cloto_thread_register();
    _this_thread = cloto_thread_get();
    memset(&_framebuffer, 0, sizeof(TerraFramebuffer));
    _paused = false;
    _tile_counter = 0;
    _target_camera = nullptr;
    _target_scene = nullptr;

    terra_profile_session_create(0, 16);
}

TerraRenderer::~TerraRenderer() {
    if (_workers != nullptr) {
        cloto_slavegroup_destroy(_workers.get());
    }

    cloto_thread_dispose();
    
    terra_profile_session_delete(0);
}

void TerraRenderer::update(
    const Scene& scene,
    const Camera& camera
) {
    if (!_bvh) {
        _on_scene_changed(scene);
        terra_framebuffer_clear(&_framebuffer);

        memcpy(&_camera_active.position.x, camera.position(), sizeof(vec3));
        memcpy(&_camera_active.direction.x, camera.direction(), sizeof(vec3));
        _camera_active.fov = 45;
        _camera_active.up = terra_f3_set(0.f, 1.f, 0.f);
        step(_camera_active, _bvh,
            [this]() {
                stbi_write_hdr("D:/data/example.hdr", _framebuffer.width, _framebuffer.height, 3, (const float*)_framebuffer.pixels);
            },
            [](size_t x, size_t y, size_t w, size_t h) {
                Log::verbose(FMT("Begin tile %llu %llu %llu %llu", x, y, w, h));
            },
            [](size_t x, size_t y, size_t w, size_t h) {
                Log::verbose(FMT("Finished tile %llu %llu %llu %llu", x, y, w, h));
            }
        );

        _camera_active.fov = 45.f;
    }

    _process_messages();

    if (_paused) {
        return;
    }

    _update_render_target_pixels();
    _render_target->upload();

    // The current job queue has finished executing, we can now read from TerraFramebuffer
    if (_tile_counter == 0) {
        _update_stats();

        // Notifying
        if (_on_step_end) {
            _on_step_end();
        }

        if (_iterative) {
            Log::verbose(FMT("Finished iteration %d", _iterations));

            if (_opt_render_change) {
                // TODO move this out, make the rendering stop asap
                Log::warning(STR("Rendering settings were changed. Call step() or loop() to begin a new rendering."));
                _paused = true;
            }
            else {
                if (_opt_job_change) {
                    _setup_threads();
                    _opt_job_change = false;
                    _push_jobs();
                }
                else {
                    _restart_jobs();
                }

                ++_iterations;
            }
        }
        else {
            Log::verbose(STR("Finished step"));
            _paused = true;
        }
    }
}

void TerraRenderer::clear() {
    terra_framebuffer_clear(&_framebuffer);
    _clear_stats();
    _clear_framebuffer = true;
}

void TerraRenderer::start() {
    Renderer::start();


}

bool TerraRenderer::step(const TerraCamera& camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end) {
    _target_scene = scene;
    _target_camera = &camera;
    _on_step_end = on_step_end;
    _on_tile_begin = on_tile_begin;
    _on_tile_end = on_tile_end;
    _iterative = false;
    return _launch();
}

bool TerraRenderer::loop(const TerraCamera& camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end) {
    if (!_paused) {
        Log::error(STR("Rendering is already in progress."));
        return false;
    }

    _target_scene = scene;
    _target_camera = &camera;
    _on_step_end = on_step_end;
    _on_tile_begin = on_tile_begin;
    _on_tile_end = on_tile_end;
    _iterative = true;
    return _launch();
}

void TerraRenderer::pause() {
    if (!_paused) {
        Log::verbose(STR("Renderer will pause at the end of current step"));
        _paused = true;
    }
    else {
        Log::verbose(STR("Renderer is already paused"));
    }
}

void TerraRenderer::update_config() {
    // We defer the actual update until a new rendering step is required
    if (_tile_size != Config::read_i(Config::JOB_TILE_SIZE)
        || _worker_count != Config::read_f(Config::JOB_N_WORKERS)
        ) {
        // Update the job system, keep the current rendering valid
        _opt_job_change = true;
    }

    if (_width != Config::read_i(Config::RENDER_WIDTH)
        || _height != Config::read_i(Config::RENDER_HEIGHT)
        ) {
        // Restart the rendering
        _opt_render_change = true;
    }
}

int TerraRenderer::iterations() const {
    return _iterations;
}

ClotoThread* TerraRenderer::thread() const {
    return _this_thread;
}

void TerraRenderer::_update_stats() {
    TERRA_PROFILE_UPDATE_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER);
    TERRA_PROFILE_UPDATE_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE);
    TERRA_PROFILE_UPDATE_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY);
    TERRA_PROFILE_UPDATE_STATS(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION);
}

void TerraRenderer::_clear_stats() {
    TERRA_PROFILE_CLEAR_TARGET(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER);
    TERRA_PROFILE_CLEAR_TARGET(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE);
    TERRA_PROFILE_CLEAR_TARGET(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY);
    TERRA_PROFILE_CLEAR_TARGET(TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION);
}

void TerraRenderer::_num_tiles(int& tiles_x, int& tiles_y) {
    int tile_size = Config::read_i(Config::Opts::JOB_TILE_SIZE);

    if (_framebuffer.pixels == nullptr || tile_size < 0) {
        Log::error(STR("Internal state not initialized"));
        return;
    }

    tiles_x = (int)ceilf((float)_framebuffer.width / tile_size);
    tiles_y = (int)ceilf((float)_framebuffer.height / tile_size);
}

void TerraRenderer::_setup_threads() {
    int workers = Config::read_i(Config::JOB_N_WORKERS);

    // Free previous allocations
    if (_workers != nullptr) {
        cloto_slavegroup_destroy(_workers.get());
        _workers = nullptr;
        TERRA_PROFILE_DELETE_SESSION(TERRA_PROFILE_SESSION_DEFAULT);
    }

    // Compute tile/queue size
    int tx, ty;
    _num_tiles(tx, ty);
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
    _workers.reset(new ClotoSlaveGroup);
    cloto_slavegroup_create(_workers.get(), workers, job_buffer_size);
    // setup profiler
    TERRA_PROFILE_CREATE_SESSION(TERRA_PROFILE_SESSION_DEFAULT, workers);
    TERRA_PROFILE_CREATE_TARGET(time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER, 0xaffff);
    TERRA_PROFILE_CREATE_TARGET(time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE, 0xaffff);
    TERRA_PROFILE_CREATE_TARGET(time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY, 0xaffff);
    TERRA_PROFILE_CREATE_TARGET(time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, 0xafffff);

    for (int i = 0; i < workers; ++i) {
        ClotoMessageJobPayload payload;
        struct {
            size_t session;
        } msg_args;
        auto msg_routine = [](void* args) -> void {
            size_t* session = (size_t*)args;
            TERRA_PROFILE_REGISTER_THREAD(*session);
        };
        payload.routine = msg_routine;
        size_t session = TERRA_PROFILE_SESSION_DEFAULT;
        memcpy(payload.buffer, &session, sizeof(session));
        cloto_thread_send_message(&_workers->slaves[i].thread, CLOTO_MSG_JOB_LOCAL_ARGS, &payload, CLOTO_MSG_PAYLOAD_SIZE);
    }

    // create jobs
    int tile_size = Config::read_i(Config::Opts::JOB_TILE_SIZE);
    int num_tiles_x, num_tiles_y;
    _num_tiles(num_tiles_x, num_tiles_y);
    _job_args.clear();
    _job_args.resize(num_tiles_x * num_tiles_y);

    for (int i = 0; i < num_tiles_y; ++i) {
        for (int j = 0; j < num_tiles_x; ++j) {
            TerraRenderArgs* args = _job_args.data() + i * num_tiles_x + j;
            args->th = this;
            args->x = j * tile_size;
            args->y = i * tile_size;
            args->width = (int)terra_mini((size_t)(j + 1) * tile_size, _framebuffer.width) - j * tile_size;
            args->height = (int)terra_mini((size_t)(i + 1) * tile_size, _framebuffer.height) - i * tile_size;
        }
    }
}

void TerraRenderer::_push_jobs() {
    int num_tiles_x, num_tiles_y;
    _num_tiles(num_tiles_x, num_tiles_y);
    _tile_counter = num_tiles_x * num_tiles_y;
    Log::verbose(FMT("Pushing %d jobs", _tile_counter));
    cloto_workqueue_clear(&_workers->queue);

    for (int i = 0; i < num_tiles_y; ++i) {
        for (int j = 0; j < num_tiles_x; ++j) {
            TerraRenderArgs* args = _job_args.data() + i * num_tiles_x + j;
            ClotoJob job;
            job.routine = &terra_render_launcher;
            job.args = args;
            cloto_workqueue_push(&_workers->queue, &job);
        }
    }
}

void TerraRenderer::_restart_jobs() {
    int tx, ty;
    _num_tiles(tx, ty);
    int n_jobs = tx * ty;
    _tile_counter = n_jobs;
    cloto_slavegroup_reset(_workers.get());
}

// Rebuilds the bvh
void TerraRenderer::_on_scene_changed(const Scene& scene) {
    _load_scene_options();

    if (_bvh) {
        terra_scene_destroy(_bvh);
    }

    _bvh = terra_scene_create();

    for (const auto& mesh : scene.objects()) {
        for (const auto& submesh : mesh.render.submeshes) {
            const size_t n_triangles = submesh.faces.count / 3;
            TerraObject* o = terra_scene_add_object(_bvh, n_triangles);

            for (size_t f = 0; f < n_triangles; ++f) {
                const uint32_t a = submesh.faces.ptr.get()[f * 3 + 0];
                const uint32_t b = submesh.faces.ptr.get()[f * 3 + 1];
                const uint32_t c = submesh.faces.ptr.get()[f * 3 + 2];
                o->triangles[f].a.x = mesh.render.x.ptr.get()[a];
                o->triangles[f].a.y = mesh.render.y.ptr.get()[a];
                o->triangles[f].a.z = mesh.render.z.ptr.get()[a];
                o->triangles[f].b.x = mesh.render.x.ptr.get()[b];
                o->triangles[f].b.y = mesh.render.y.ptr.get()[b];
                o->triangles[f].b.z = mesh.render.z.ptr.get()[b];
                o->triangles[f].c.x = mesh.render.x.ptr.get()[c];
                o->triangles[f].c.y = mesh.render.y.ptr.get()[c];
                o->triangles[f].c.z = mesh.render.z.ptr.get()[c];
                o->properties[f].normal_a.x = mesh.render.nx.ptr.get()[a];
                o->properties[f].normal_a.y = mesh.render.ny.ptr.get()[a];
                o->properties[f].normal_a.z = mesh.render.nz.ptr.get()[a];
                o->properties[f].normal_b.x = mesh.render.nx.ptr.get()[b];
                o->properties[f].normal_b.y = mesh.render.ny.ptr.get()[b];
                o->properties[f].normal_b.z = mesh.render.nz.ptr.get()[b];
                o->properties[f].normal_c.x = mesh.render.nx.ptr.get()[c];
                o->properties[f].normal_c.y = mesh.render.ny.ptr.get()[c];
                o->properties[f].normal_c.z = mesh.render.nz.ptr.get()[c];
                o->properties[f].texcoord_a.x = 0.f;
                o->properties[f].texcoord_a.y = 0.f;
                o->properties[f].texcoord_b.x = 0.f;
                o->properties[f].texcoord_b.y = 0.f;
                o->properties[f].texcoord_c.x = 0.f;
                o->properties[f].texcoord_c.y = 0.f;
            }

            o->material.ior = 1.5;
            o->material.enable_bump_map_attr = 0;
            o->material.enable_normal_map_attr = 0;

            TerraFloat3 albedo = terra_f3_set(1.f, 0.f, 1.f);
            terra_attribute_init_constant(o->material.attributes + TERRA_DIFFUSE_ALBEDO, &albedo);
            o->material.attributes_count = TERRA_DIFFUSE_END;
            terra_bsdf_diffuse_init(&o->material.bsdf);
        }
    }

    TerraSceneOptions* opts = terra_scene_get_options(_bvh);
    *opts = _opts;
    terra_scene_commit(_bvh);
}

void TerraRenderer::_load_scene_options() {
    string tonemap_str = Config::read_s(Config::RENDER_TONEMAP);
    string accelerator_str = Config::read_s(Config::RENDER_ACCELERATOR);
    string sampling_str = Config::read_s(Config::RENDER_SAMPLING);
    string integrator_str = Config::read_s(Config::RENDER_INTEGRATOR);
    TerraTonemappingOperator tonemap = Config::to_terra_tonemap(tonemap_str);
    TerraAccelerator accelerator = Config::to_terra_accelerator(accelerator_str);
    TerraSamplingMethod sampling = Config::to_terra_sampling(sampling_str);
    TerraIntegrator integrator = Config::to_terra_integrator(integrator_str);

    if (tonemap == -1) {
        Log::error(FMT("Invalid configuration RENDER_TONEMAP value %s. Defaulting to none.", tonemap_str.c_str()));
        tonemap = kTerraTonemappingOperatorNone;
    }

    if (accelerator == -1) {
        Log::error(FMT("Invalid configuration RENDER_ACCELERATOR value %s. Defaulting to BVH.", accelerator_str.c_str()));
        accelerator = kTerraAcceleratorBVH;
    }

    if (sampling == -1) {
        Log::error(FMT("Invalid configuration RENDER_SAMPLING value %s. Defaulting to random.", sampling_str.c_str()));
        sampling = kTerraSamplingMethodRandom;
    }

    if (integrator == -1) {
        Log::error(FMT("Invalid configuration RENDER_INTEGRATOR value %s. Defaulting to simple.", integrator_str.c_str()));
        integrator = kTerraIntegratorSimple;
    }

    int bounces = Config::read_i(Config::RENDER_MAX_BOUNCES);
    int samples = Config::read_i(Config::RENDER_SAMPLES);
    float exposure = Config::read_f(Config::RENDER_EXPOSURE);
    float gamma = Config::read_f(Config::RENDER_GAMMA);
    float jitter = Config::read_f(Config::RENDER_JITTER);

    if (bounces < 0) {
        Log::error(FMT("Invalid configuration RENDER_MAX_BOUNCES (%d < 0). Defaulting to 64.", bounces));
        bounces = 64;
    }

    if (samples < 0) {
        Log::error(FMT("Invalid configuration RENDER_SAMPLES (%d < 0). Defaulting to 8.", samples));
        samples = 8;
    }

    if (exposure < 0) {
        Log::error(FMT("Invalid configuration RENDER_EXPOSURE (%f < 0). Defaulting to 1.0.", exposure));
        exposure = 1.0;
    }

    if (gamma < 0) {
        Log::error(FMT("Invalid configuration RENDER_GAMMA (%f < 0). Defaulting to 2.2.", gamma));
        gamma = 2.2f;
    }

    if (jitter < 0) {
        Log::error(FMT("Invalid configuration RENDER_JITTER (%f < 0) Defaulting to 0.", jitter));
        jitter = 0;
    }

    _opts.bounces = bounces;
    _opts.samples_per_pixel = samples;
    _opts.subpixel_jitter = jitter;
    _opts.tonemapping_operator = tonemap;
    _opts.manual_exposure = exposure;
    _opts.gamma = gamma;
    _opts.accelerator = accelerator;
    _opts.strata = 4;
    _opts.sampling_method = sampling;
    _opts.integrator = integrator;
    TerraFloat3 envmap_color = terra_f3_set(1.f, 1.f, 1.f);
    terra_attribute_init_constant(&_opts.environment_map, &envmap_color);
}

void TerraRenderer::_update_render_target_pixels() {
    assert(_framebuffer.width == _render_target->width);
    assert(_framebuffer.height == _render_target->height);
    const int cols = _render_target->width;
    const int rows = _render_target->height;

    if (_render_target->format == GL_RGBA32F) {
        memcpy(_render_target->pixels.get(), 
            _framebuffer.pixels, rows * cols * _render_target->stride);
    }
    else {
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                const TerraFloat3* pixel_r  = _framebuffer.pixels + (r * cols + c);
                uint8_t* pixel_w = _render_target->pixels.get() + (r * cols + c) * _render_target->stride;

                switch (_render_target->format) {
                case GL_RGBA8:
                    pixel_w[0] = (uint8_t)(255 * pixel_r->x);
                    pixel_w[1] = (uint8_t)(255 * pixel_r->y);
                    pixel_w[2] = (uint8_t)(255 * pixel_r->z);
                    pixel_w[3] = 1.0;
                    break;
                default:
                    assert(false);
                    break;
                }
            }
        }
    }
}

bool TerraRenderer::_launch() {
    //if ( _framebuffer.pixels == nullptr ) {
    //    Log::error ( STR ( "Cannot launch rendering, invalid destination Terra framebuffer" ) );
    //    return false;
    //}
    assert(_target_camera);
    assert(_target_scene);
    //assert(_paused);

    // Sync config
    if (_opt_render_change) {
        if (_framebuffer.pixels != nullptr) {
            terra_framebuffer_destroy(&_framebuffer);
            memset(&_framebuffer, 0, sizeof(TerraFramebuffer));
        }

        int width = Config::read_i(Config::Opts::RENDER_WIDTH);
        int height = Config::read_i(Config::Opts::RENDER_HEIGHT);

        if (!terra_framebuffer_create(&_framebuffer, width, height)) {
            Log::error(STR("Failed to create Terra framebuffer, subsequent calls will fail"));
            return false;
        }

        Log::verbose(FMT("Creating Terra framebuffer %dx%d", _framebuffer.width, _framebuffer.height));
        Log::verbose(STR("Creating Terra render jobs"));
        // Creating workers and jobs (required on framebuffer size change)
        _setup_threads();
        // Update camera
    }
    else if (_opt_job_change) {
        _setup_threads();
    }

    _opt_job_change = false;
    _opt_render_change = false;
    // Setup internal state
    _paused = false;
    _iterations = 0;
    _clear_framebuffer = false;
    // Push jobs
    _push_jobs();
    return true;
}

void TerraRenderer::_process_messages() {
    cloto_thread_process_messages(_this_thread);
}
