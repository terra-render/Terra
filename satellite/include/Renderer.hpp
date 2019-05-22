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
#include <Config.hpp>

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

    // Call this inside the main loop to progress the rendering, after starting one with step/loop.
    void update();

    // Clears the framebuffer.
    void clear();

    // Step the rendering.
    bool step ( const TerraCamera& camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end );

    // Keep stepping until further input (e.g. pause).
    bool loop ( const TerraCamera& camera, HTerraScene scene, const Event& on_step_end, const TileEvent& on_tile_begin, const TileEvent& on_tile_end );

    // Pause the current rendering.
    void pause();

    // Call this to notify the renderer of changes to config.
    void update_config();

    // Getters
    const TextureData&          framebuffer();
    bool                        is_framebuffer_clear() const;
    int                         iterations() const;
    ClotoThread*                thread() const;

  private:
    bool     _launch();
    void     _setup_threads();
    void     _push_jobs();
    void     _restart_jobs();
    void     _update_stats();
    void     _clear_stats();
    void     _process_messages();

    void     _num_tiles ( int& tiles_x, int& tiles_y ); // Calculates the number of tiles from the current framebuffer / tile_size

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
    std::vector<TerraRenderArgs>     _job_args;

    // Renderer state
    bool         _opt_render_change = true;
    bool         _opt_job_change;
    bool         _paused;
    bool         _iterative;
    bool         _clear_framebuffer = true;
    int          _iterations;
    Event        _on_step_end;   // Also called at the end of every loop iteration
    TileEvent    _on_tile_begin;
    TileEvent    _on_tile_end;
    const TerraCamera* _target_camera;
    HTerraScene  _target_scene;
    std::mutex   _callback_mutex;
    // Config
    int _width;
    int _height;
    int _tile_size;
    int _worker_count;
};