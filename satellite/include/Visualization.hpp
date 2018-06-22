#pragma once

// stdlib
#include <cstdint>
#include <memory>
#include <map>
#include <functional>

// Satellite
#include <Graphics.hpp>
#include <Console.hpp>

//
// Does not hold any reference to the render targets data
// Manages every visualization on screen except for the console
//
class Visualizer {
  public:
    void init ( GFXLayer gfx );

    void set_texture_data ( const TextureData& data );
    void update_tile ( const TextureData& data, size_t x, size_t y, size_t w, size_t h );
    void save_to_file ( const char* path );

    // The different views are not hardcoded as they are used for debugging Terra
    // internals and often edited.
    void toggle_debug_view ( const char* name );

    // Draws the framebuffer and debug views
    void draw();

  private:
    GFXLayer _gfx;

    TextureData  _texture;
    int          _gl_format;
    unsigned int _gl_texture;

    bool         _hide_info;

    using DebugView = void ( * ) ( bool );
    std::vector<DebugView> _dbg_views;
};