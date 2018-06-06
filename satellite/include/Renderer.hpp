#pragma once

// C++ STL
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

// Terra
#include <Terra.h>

// Cloto
#include <Cloto.h>
#include <Graphics.hpp>

//
// Dispatches terra_render jobs
//
class TerraRenderer {
  public:
    using Event = std::function<void() >;
    using TileEvent = std::function<void ( size_t x, size_t y, size_t w, size_t h ) >;

  public:
    TerraRenderer ();
    ~TerraRenderer();

    bool init ( int width, int height, int tile_side_length = 8, int concurrent_jobs = 8 );

    // call init() once and the following setters when something changes. Note that the changes
    // are lazy, they are applied right before new jobs are pushed. Even if paused() the internal
    // state is not changed and it can be safely read (e.g. framebuffer())
    void resize ( int width, int height );
    void set_concurrent_jobs ( int concurrent_jobs );
    void set_tile_size ( int tile_side_length );
    void set_progressive ( bool progressive );

    // Takes care of pushing new jobs if iterative() is true; processing
    // option changes and notying steps.
    void refresh_jobs();

    // Any call to step or loop resumes the rendering (unpausing).
    // Returns true if jobs were successfully launched
    bool step ( TerraCamera* camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end );
    bool loop ( TerraCamera* camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end );

    // Pauses rendering once the current step has finished executing
    void pause_at_next_step();

    // Get framebuffer data
    const TextureData&       framebuffer();
    const TerraSceneOptions& options() const;

    // Renderer state
    bool is_rendering() const;
    bool is_paused() const;
    bool is_iterative() const;
    bool is_progressive() const;
    int  iterations() const;

    int concurrent_jobs() const;
    int tile_size() const;
    ClotoThread* thread() const;

  private:
    void     _create_jobs();
    void     _push_jobs();
    void     _num_tiles ( int& tiles_x, int& tiles_y ); // Calculates the number of tiles from the current framebuffer / tile_size
    uint64_t _gen_scene_id ( const TerraCamera* camera, HTerraScene scene );
    bool     _state_changed(); // True fi
    bool     _apply_changes();
    bool     _launch ();
    void     _process_messages();

    typedef struct TerraRenderArgs {
        TerraRenderer*  th;
        int             x, y;
        int             width, height;
    } TerraRenderArgs;
    friend void terra_render_launcher ( void* );

    // Threading
    std::unique_ptr<ClotoSlaveGroup> _workers;
    uint32_t                         _tile_counter;
    ClotoThread*                     _this_thread;

    // Terra
    TerraFramebuffer                 _framebuffer;
    TextureData                      _framebuffer_data;
    TerraSceneOptions                _options;
    std::vector<TerraRenderArgs>     _job_args;

    // Renderer state
    bool         _paused;
    bool         _iterative;
    bool         _progressive;
    int          _tile_size;
    int          _concurrent_jobs;
    int          _iterations;
    Event        _on_step_end;   // Also called at the end of every loop iteration
    TileEvent    _on_tile_begin;
    TileEvent    _on_tile_end;
    TerraCamera* _target_camera;
    HTerraScene  _target_scene;
    std::mutex   _callback_mutex;

    // Cached here and applied when rendering pauses
    uint64_t _scene_id; // hash(scene) ^ hash(camera)
    int      _next_tile_size;
    int      _next_concurrent_jobs;
    int      _next_width;
    int      _next_height;
};