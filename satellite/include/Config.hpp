#pragma once

// C++ STL
#include <string>

// Render options that can be set from the terminal through `option set <name> <value>`
#define RENDER_OPT_WORKERS_DESC     "Number of worker threads"
#define RENDER_OPT_WORKERS_NAME     "workers"

#define RENDER_OPT_TILE_SIZE_DESC "Side length in pixels of a rendering job"
#define RENDER_OPT_TILE_SIZE_NAME "tile-size"
#define RENDER_OPT_TILE_SIZE_DEFAULT 128

#define RENDER_OPT_BOUNCES_DESC "Maximum ray bounces (-1 for unbounded)"
#define RENDER_OPT_BOUNCES_NAME "bounces"
#define RENDER_OPT_BOUNCES_DEFAULT 4

#define RENDER_OPT_SAMPLES_DESC "Number of samples per pixel"
#define RENDER_OPT_SAMPLES_NAME "samples"
#define RENDER_OPT_SAMPLES_DEFAULT 8

#define RENDER_OPT_GAMMA_DESC "Output display gamma for final color correction"
#define RENDER_OPT_GAMMA_NAME "gamma"
#define RENDER_OPT_GAMMA_DEFAULT ((float)(2.2))

#define RENDER_OPT_EXPOSURE_DESC "Manual camera exposure"
#define RENDER_OPT_EXPOSURE_NAME "exposure"
#define RENDER_OPT_EXPOSURE_DEFAULT ((float)1.)

// See kTerraTonemappingOperator
#define RENDER_OPT_TONEMAP_DESC "Tonemapping operator [none|linear|filmic|reinhard|uncharted]"
#define RENDER_OPT_TONEMAP_NAME "tonemap"
#define RENDER_OPT_TONEMAP_NONE "none"
#define RENDER_OPT_TONEMAP_LINEAR "linear"
#define RENDER_OPT_TONEMAP_REINHARD "reinhard"
#define RENDER_OPT_TONEMAP_FILMIC "filmic"
#define RENDER_OPT_TONEMAP_UNCHARTED2 "uncharted"
#define RENDER_OPT_TONEMAP_DEFAULT RENDER_OPT_TONEMAP_LINEAR

#define RENDER_OPT_SAMPLER_DESC "Monte carlo sampler [random|stratified|halton]"
#define RENDER_OPT_SAMPLER_NAME "sampler"
#define RENDER_OPT_SAMPLER_RANDOM "random"
#define RENDER_OPT_SAMPLER_STRATIFIED "stratified"
#define RENDER_OPT_SAMPLER_HALTON "halton"
#define RENDER_OPT_SAMPLER_DEFAULT RENDER_OPT_SAMPLER_RANDOM

#define RENDER_OPT_ACCELERATOR_DESC "Intersection acceleration structure [bvh]"
#define RENDER_OPT_ACCELERATOR_NAME "accelerator"
#define RENDER_OPT_ACCELERATOR_BVH "bvh"
#define RENDER_OPT_ACCELERATOR_DEFAULT RENDER_OPT_ACCELERATOR_BVH

#define RENDER_OPT_WIDTH_DESC "Render width"
#define RENDER_OPT_WIDTH_NAME "width"
#define RENDER_OPT_WIDTH_DEFAULT 800

#define RENDER_OPT_HEIGHT_DESC "Render height"
#define RENDER_OPT_HEIGHT_NAME "height"
#define RENDER_OPT_HEIGHT_DEFAULT 600

#define RENDER_OPT_PROGRESSIVE_DESC "Update the display every time a job finishes"
#define RENDER_OPT_PROGRESSIVE_NAME "progressive"
#define RENDER_OPT_PROGRESSIVE_DEFAULT 1

#define RENDER_OPT_CAMERA_POS_DESC "Camera position"
#define RENDER_OPT_CAMERA_POS_NAME "campos"
#define RENDER_OPT_CAMERA_POS_DEFAULT { 0.f, 0.9f, 2.3f }

#define RENDER_OPT_CAMERA_DIR_DESC "Camera direction"
#define RENDER_OPT_CAMERA_DIR_NAME "camdir"
#define RENDER_OPT_CAMERA_DIR_DEFAULT { 0.f, 0.f, 1.f }

#define RENDER_OPT_CAMERA_UP_DESC "Camera up"
#define RENDER_OPT_CAMERA_UP_NAME "camup"
#define RENDER_OPT_CAMERA_UP_DEFAULT { 0.f, 1.f, 0.f }

#define RENDER_OPT_CAMERA_VFOV_DEG_DESC "Camera vertical field of view in degrees"
#define RENDER_OPT_CAMERA_VFOV_DEG_NAME "camfov"
#define RENDER_OPT_CAMERA_VFOV_DEG_DEFAULT 45.f

#define RENDER_OPT_SCENE_PATH_DESC "Scene file path"
#define RENDER_OPT_SCENE_PATH_NAME "scene"
#define RENDER_OPT_SCENE_PATH_DEFAULT "scene.obj"

#define RENDER_OPT_ENVMAP_COLOR_DESC "Envmap color"
#define RENDER_OPT_ENVMAP_COLOR_NAME "envmap"
#define RENDER_OPT_ENVMAP_COLOR_DEFAULT { 0.4f, 0.52f, 1.f }

//
// Config wraps any configurable bit of the app.
// Can be safely read/written from anywhere, although writing should probably
// be done from a single source.
//
// It loads the options with default values and also reading from a text file
// called `satellite.config` (CONFIG_PATH) in the current directory, or parent or data/
//
// Options are conceptually separated in:
//  - (CMD_)    Parses command line arguments for commands to execute at App::_boot().
//  - (RENDER_) Handles Terra rendering options. (Although not aware of Terra types)
//  - (K_)      Handles any other app constant.
// It also resolves string => options mappings (console) and types.
// There is no need for this to be dynamic, all options are know at compile time.
//
#define CONFIG_PATH "satellite.config"

namespace Config {
    enum Opts {
        OPTS_NONE = -1,

        JOB_N_WORKERS = 0,
        JOB_TILE_SIZE,

        RENDER_MAX_BOUNCES,
        RENDER_SAMPLES,
        RENDER_GAMMA,
        RENDER_EXPOSURE,
        RENDER_TONEMAP,
        RENDER_ACCELERATOR,
        RENDER_SAMPLING,
        RENDER_WIDTH,
        RENDER_HEIGHT,
        RENDER_SCENE_PATH,
        RENDER_CAMERA_POS,
        RENDER_CAMERA_DIR,
        RENDER_CAMERA_UP,
        RENDER_CAMERA_VFOV_DEG,
        RENDER_ENVMAP_COLOR,

        VISUALIZER_PROGRESSIVE,

        OPTS_COUNT
    };

    enum class Type {
        None = -1,

        Int,
        Real,
        Real3,
        Str,
        Exec,

        Count
    };

    bool init ();
    void dump ( int from = -1, int to = -1 );
    void dump_desc ( int from = -1, int to = -1 );
    void reset_to_default();
    bool save ( const char* path );
    bool save();
    bool load ( const char* path );
    bool load();

    Type type ( int opt );              // Opt  => type
    Type type ( const char* name );     // name => type
    int  find ( const char* name );     // name => Opt

    int         read_i ( int opt );
    float       read_f ( int opt );
    bool        read_f3 ( int opt, float* f3 );
    std::string read_s ( int opt );

    void write ( int opt, const std::string& val );
    void write_i ( int opt, int val );
    void write_f ( int opt, float val );
    void write_f3 ( int opt, float* f3 );
    void write_s ( int opt, const char* val );

    bool parse_i ( const char* s, int& v );
    bool parse_f ( const char* s, float& v );
    bool parse_f3 ( const char* s, float* f3 );
}