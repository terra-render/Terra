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

    // Interrupt the current render
    void interrupt();

    // Wait for the current render to finish
    void stop();

    // Call this to notify the renderer of changes to config.
    void update_config();

    // Getters
    const TextureData&          framebuffer();
    bool                        is_framebuffer_clear() const;
    int                         iterations() const;
    ClotoThread*                thread() const;

    int get_frame_idx() { return _frame_idx; }
    int get_tile_size() { return _tile_size;  };
    TerraFramebuffer* get_terra_framebuffer() { return &_framebuffer; }

  private:
    bool     _launch();
    void     _setup_threads();
    void     _push_jobs();
    void     _restart_jobs();
    void     _update_stats();
    void     _clear_stats();
    void     _process_messages();

    void     _num_tiles ( int& tiles_x, int& tiles_y, int tile_size ); // Calculates the number of tiles from the current framebuffer / tile_size

    typedef struct TerraRenderArgs {
        TerraRenderer*  renderer;
        int             x, y;
        int             width, height;
        int             frame;
    } TerraRenderArgs;
    friend void terra_render_launcher ( void* );

    // Threading
    std::unique_ptr<ClotoSlaveGroup> _workers;
    uint32_t                         _tile_counter;
    ClotoThread*                     _this_thread;

    // Terra
    TerraFramebuffer                 _framebuffer;
    std::mutex                       _framebuffer_mutex;

    TextureData                      _framebuffer_data;
    std::vector<TerraRenderArgs>     _job_args;

    // Renderer state
    bool         _opt_render_change = true;
    bool         _opt_job_change;
    bool         _stopped;
    bool         _iterative;
    bool         _clear_framebuffer = true;
    int          _iterations;
    Event        _on_step_end;   // Also called at the end of every loop iteration
    TileEvent    _on_tile_begin;
    TileEvent    _on_tile_end;


    const TerraCamera* _target_camera;
    HTerraScene  _target_scene;

    // Active frame we are displaying, old running jobs can use this to determine
    // whether to discard the output
    uint64_t _frame_idx;

    // Config
    int _tile_size; // for the current frame
    int _width;
    int _height;
    int _worker_count;
};