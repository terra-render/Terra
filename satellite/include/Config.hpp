#pragma once

// C++ STL
#include <string>

// Render options that can be set from the terminal through `option set <name> <value>`
#define RENDER_OPT_WORKERS_DESC    "Number of worker threads concurrently rendering"
#define RENDER_OPT_WORKERS_NAME    "workers"
#define RENDER_OPT_WORKERS_DEFAULT 16 // -1 to use all possible threads

#define RENDER_OPT_TILE_SIZE_DESC "Side length of a rendering job. Dimension is in pixels"
#define RENDER_OPT_TILE_SIZE_NAME "tile-size"
#define RENDER_OPT_TILE_SIZE_DEFAULT 128

#define RENDER_OPT_BOUNCES_DESC "Maximum ray bounces (if Russian Rulette is activated, this option is ignored)"
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
#define RENDER_OPT_TONEMAP_DESC "Tonemapping operator"
#define RENDER_OPT_TONEMAP_NAME "tonemap"
#define RENDER_OPT_TONEMAP_NONE "none"
#define RENDER_OPT_TONEMAP_LINEAR "linear"
#define RENDER_OPT_TONEMAP_REINHARD "reinhard"
#define RENDER_OPT_TONEMAP_FILMIC "filmic"
#define RENDER_OPT_TONEMAP_UNCHARTED2 "uncharted"
#define RENDER_OPT_TONEMAP_DEFAULT RENDER_OPT_TONEMAPE_LINEAR

#define RENDER_OPT_SAMPLER_DESC "Sampling strategy for the monte carlo integration"
#define RENDER_OPT_SAMPLER_NAME "sampler"
#define RENDER_OPT_SAMPLER_RANDOM "random"
#define RENDER_OPT_SAMPLER_STRATIFIED "stratified"
#define RENDER_OPT_SAMPLER_HALTON "halton"

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

        CMD_CONFIG,
        CMD_LOAD,
        CMD_RENDER,
        CMD_SAVE,

        JOB_N_WORKERS,
        JOB_TILE_SIZE,

        RENDER_MAX_BOUNCES,
        RENDER_SAMPLES,
        RENDER_GAMMA,
        RENDER_EXPOSURE,
        RENDER_TONEMAP,
        RENDER_ACCELERATOR,
        RENDER_SAMPLING,

        OPTS_COUNT
    };

    enum class Type {
        None = -1,

        Int,
        Real,
        Str,
        Exec,

        Count
    };

    bool init ();
    void dump ( int from = -1, int to = -1 );
    bool save ( const char* path );
    bool save();
    bool load ( const char* path );
    bool load();

    Type type ( int opt );              // Opt  => type
    Type type ( const char* name );     // name => type
    int  find ( const char* name );     // name => Opt

    // More clear without templates
    int         read_i ( int opt );
    float       read_f ( int opt );
    std::string read_s ( int opt ); // copy to avoid reference to internal data structure which can be written to.

    void write_i ( int opt, int val );
    void write_f ( int opt, float val );
    void write_s ( int opt, const char* val );

    bool parse_i ( const char* s, int& v );
    bool parse_f ( const char* s, float& v );
}