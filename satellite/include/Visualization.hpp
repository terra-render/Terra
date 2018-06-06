#pragma once

// stdlib
#include <cstdint>
#include <memory>
#include <map>

// Satellite
#include <Graphics.hpp>
#include <Console.hpp>

//
// Does not hold any reference to the render targets data
// Manages every visualization on screen except for the console
//
class Visualizer {
  public:
    // Information panel
    // just the useful bits
    struct Info {
        std::string scene;
        int         spp;
        std::string accelerator;
        std::string sampling;
    };

  public:
    void init ( GFXLayer gfx );

    void set_texture_data ( const TextureData& data );
    void save_to_file ( const char* path );

    void toggle_info();

    void draw();

    Info&  info();

  private:
    GFXLayer _gfx;

    Info         _info;
    TextureData  _texture;
    int          _gl_format;
    unsigned int _gl_texture;

    bool         _hide_info;
};