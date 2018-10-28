#pragma once

// stdlib
#include <cstdint>
#include <memory>
#include <map>
#include <vector>

// Satellite
#include <Graphics.hpp>
#include <Console.hpp>

// Terra
#include <TerraProfile.h>

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

    struct Stats {
        size_t session;
        size_t target;
        std::string name;
        TerraProfileSampleType type;
        TerraProfileStats data;
    };

  public:
    void init ( GFXLayer* gfx );

    void set_texture_data ( const TextureData& data );
    void update_tile ( const TextureData& data, size_t x, size_t y, size_t w, size_t h );

    void save_to_file ( const char* path );

    void toggle_info();

    void add_stats_tracker ( size_t session, size_t target, const char* name, TerraProfileSampleType type );
    void remove_stats_tracker ( const char* name );
    void remove_all_stats_trackers();
    void update_stats();

    void draw();

    Info&               info();
    std::vector<Stats>& stats();

  private:
    GFXLayer*    _gfx;

    Info         _info;
    TextureData  _texture;
    int          _gl_format;
    unsigned int _gl_texture;

    bool         _hide_info;

    std::vector<Stats> _stats;
};