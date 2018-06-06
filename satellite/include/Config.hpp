#pragma once

// C++ STL
#include <string>

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
    void save ( const char* path );

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