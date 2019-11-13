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
#include <Terra.h>
#include <TerraProfile.h>

// GL
#include <GL/GL.h>

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
#ifdef TERRA_PROFILE
        TerraProfileSampleType type;
        TerraProfileStats data;
#endif
    };

    struct Vertex {
        TerraFloat3 pos;
        TerraFloat3 norm;
    };

  public:
    void init ( GFXLayer* gfx );

    void set_display_image(const ImageID image);
    void set_texture_data ( const TextureData& data );
    void update_tile ( const TextureData& data, size_t x, size_t y, size_t w, size_t h );

    void save_to_file ( const char* path );

    void toggle_info();
    void add_stats_tracker ( size_t session, size_t target, const char* name );
    void remove_stats_tracker ( const char* name );
    void remove_all_stats_trackers();
    void update_stats();

    void update_config();

    void draw();

    Info&               info();
    std::vector<Stats>& stats();

  private:
    void _create_texture ( int width, int height, int gl_format, void* data );
    void _read_config();

    GFXLayer*    _gfx;

    Info         _info;
    TextureData  _texture;
    int          _gl_format;
    unsigned int _gl_texture;

    TerraAccelerator _accelerator;
    TerraSamplingMethod _sampling;

    bool         _hide_info;

    std::vector<Stats> _stats;
    ImageID _image = 0;
};