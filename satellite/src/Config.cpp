// Header
#include <Config.hpp>

// Satellite
#include <Logging.hpp>

// C++ STL
#include <cassert>
#include <thread>
#include <fstream>
#include <regex>
#include <limits>
#include <string>
#include <sstream>
#include <memory>
#include <map>
#include <shared_mutex>
#include <iomanip>

using namespace std;

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif



namespace Config {
    namespace {
        struct Opt {
            Opt ( Type type, const char* name, const char* desc ) :
                type ( type ), name ( name ), desc ( desc ) {
                if ( type == Type::Str ) {
                    v.s = nullptr;
                }
            }

            Opt() {
                v.s = nullptr;
            }

            ~Opt() {
                if ( type == Type::Str && v.s != nullptr ) {
                    free ( v.s );
                    v.s = nullptr;
                }
            }

            Opt& operator= ( const Opt& other ) {
                if ( type == Type::Str && v.s != nullptr ) {
                    free ( v.s );
                }

                name = other.name;
                desc = other.desc;
                type = other.type;

                if ( type == Type::Str ) {
                    v.s = _strdup ( other.v.s );
                } else if ( type == Type::Int ) {
                    v.i = other.v.i;
                } else if ( type == Type::Real ) {
                    v.r = other.v.r;
                }

                return *this;
            }

            Type        type = Type::None;
            const char* name;
            const char* desc;

            union {
                int    i;
                float  r;
                char*  s;
            } v;
        };

        const char* type_str[ ( size_t ) Type::Count] {
            "Int", "Real", "String", "Exec"
        };

        // Read only options
        unique_ptr<Opt[]> opts;
        map<string, int>  opts_map;
        shared_mutex      opts_lock;

        bool add_opt ( int idx, Type type, const char* name, const char* desc, Opt** opt = nullptr ) {
            if ( opts[idx].type != Type::None || opts_map.find ( name ) != opts_map.end() ) {
                Log::warning ( "Duplicated option %s", name );
                return false;
            }

            opts[idx] = Opt ( type, name, desc );
            opts_map[name] = idx;

            if ( opt ) {
                *opt = &opts[idx];
            }

            return true;
        }

        void add_opt ( int idx, int val, const char* name, const char* desc ) {
            Opt* opt;

            if ( add_opt ( idx, Type::Int, name, desc, &opt ) ) {
                opt->v.i = val;
            }
        }

        void add_opt ( int idx, float val, const char* name, const char* desc ) {
            Opt* opt;

            if ( add_opt ( idx, Type::Real, name, desc, &opt ) ) {
                opt->v.r = val;
            }
        }

        void add_opt ( int idx, const char* val, const char* name, const char* desc ) {
            Opt* opt;

            if ( add_opt ( idx, Type::Str, name, desc, &opt ) ) {
                opt->v.s = _strdup ( val );
            }
        }

        bool file_exists ( const char* path, ifstream& fs ) {
            fs.open ( path );
            return fs.good();
        }

        bool find_config_file ( ifstream& fs ) {
            if ( file_exists ( CONFIG_PATH, fs ) ) {
                return true;
            }

            if ( file_exists ( "../" CONFIG_PATH, fs ) ) {
                return true;
            }

            if ( file_exists ( "data" CONFIG_PATH, fs ) ) {
                return true;
            }

            return false;
        }

        bool next_line ( ifstream& fs, string& name, string& value ) {
            string line;
            getline ( fs, line );

            if ( !fs.good() ) {
                return false;
            }

            const char* p = line.data();

            // Finding separator
            while ( *p != '\0' && *p != '=' ) {
                ++p;
            }

            if ( *p == '\0' ) {
                Log::warning ( FMT ( "Unrecognized name=value format for line %s", line.c_str() ) );
                return true;
            }

            // Extracting value
            ++p; // skipping =
            const char* v = p;

            while ( isspace ( *v ) && *v != '"' ) { // trimming left
                ++v;
            }

            value = v;

            while ( isspace ( value.back() ) && value.back() != '"' ) { // trimming right
                value.pop_back();
            }

            // Extracting name
            const char* n = line.data();

            while ( *n != '\0' && isspace ( *n ) ) {
                ++n;
            }

            name = n;
            --p;

            for ( int i = 0; i < &line.back() - p; ++i ) {
                name.pop_back();
            }

            while ( isspace ( name.back() ) || name.back() == '=' ) {
                name.pop_back();
            }

            auto ss = name.back();
            return true;
        }
    }

    bool init ( ) {
        unique_lock<shared_mutex> ( opts_lock );
        opts = unique_ptr<Opt[]> ( new Opt[OPTS_COUNT] );
        add_opt ( CMD_CONFIG,           Type::Str,      "config",       "Read render configuration options from file." );
        add_opt ( CMD_LOAD,             Type::Str,      "load",         "Load specified scene at startup." );
        add_opt ( CMD_RENDER,           Type::Exec,     "render",       "Renders the loaded scene." );
        add_opt ( CMD_SAVE,             Type::Str,      "save",         "Saves the rendererd scene to the specified file." );
        int n_threads =  ( int ) thread::hardware_concurrency();
        add_opt ( JOB_N_WORKERS,        1,      "workers",      "Number of worker threads used by the renderer." );
        add_opt ( JOB_TILE_SIZE,        16,             "tile_size",    "Side length of area to be processed by a single worker." );
        add_opt ( RENDER_MAX_BOUNCES,   2,              "bounces",      "Maximum ray bounces" );
        add_opt ( RENDER_SAMPLES,       1,              "samples",      "Samples per pixel" );
        add_opt ( RENDER_GAMMA,         2.2f,           "gamma",        "Gamma" );
        add_opt ( RENDER_EXPOSURE,      1.f,            "exposure",     "Manual exposure" );
        add_opt ( RENDER_TONEMAP,       "linear",       "tonemap",      "Tonemapping operator [none|linear|reinhard|filmic|uncharted2]" );
        add_opt ( RENDER_ACCELERATOR,   "bvh",          "accelerator",  "Intersection acceleration structure [bvh|kdtree]" );
        add_opt ( RENDER_SAMPLING,      "random",       "sampling",     "Sampling mode [random|stratified|halton]" );
        //
        // Configuration file
        // each line is split at the first = into <name>=<value>
        // ignore line with #
        // warning are generated if no = is present
        //
        ifstream config_fs;

        if ( !find_config_file ( config_fs ) ) {
            Log::info ( STR ( "No configuration file loaded." ) );
            return true;
        }

        string name, value;

        while ( next_line ( config_fs, name, value ) ) {
            int opt = find ( name.c_str() );

            if ( opt == -1 ) {
                Log::error ( FMT ( "Unrecognized option name %s", name.c_str() ) );
                continue;
            }

            Opt opt_val;

            switch ( opts[opt].type ) {
                case Type::Int: {
                    int v;

                    if ( !parse_i ( value.c_str(), v ) ) {
                        Log::error ( FMT ( "Argument %s expects an integral, but found %s", value.c_str() ) );
                    } else {
                        opt_val.v.i = v;
                    }

                    break;
                }

                case Type::Real: {
                    float v;

                    if ( !parse_f ( value.c_str(), v ) ) {
                        Log::error ( FMT ( "Argument %s expects a real number, but found %s", value.c_str() ) );
                    } else {
                        opt_val.v.r = v;
                    }

                    break;
                }

                case Type::Str: {
                    opt_val.v.s = _strdup ( value.c_str() ); // Have fun
                    break;
                }

                case Type::Exec: {
                    if ( !value.empty() ) {
                        Log::warning ( FMT ( "Arguments %s expects no value, but %s was found", value.c_str() ) );
                    }

                    break;
                }

                default:
                    assert ( false );
            }
        }

        dump();
        return true;
    }

    void dump ( int from, int to ) {
        if ( from == -1 ) {
            from = 0;
        }

        if ( to == -1 ) {
            to = OPTS_COUNT;
        }

        assert ( from >= 0 && from <= OPTS_COUNT );
        assert ( to >= 0 && to <= OPTS_COUNT );

        for ( int i = from; i < to; ++i ) {
            Opt& opt = opts[i];

            switch ( opts[i].type ) {
                case Type::Int:
                    Log::info ( FMT ( "%s = %d", opts[i].name, opts[i].v.i ) );
                    break;

                case Type::Real:
                    Log::info ( FMT ( "%s = %f", opts[i].name, opts[i].v.r ) );
                    break;

                case Type::Str:
                    Log::info ( FMT ( "%s = %s", opts[i].name, opts[i].v.s ) );
                    break;

                default:
                    break;
            }
        }
    }

    void save ( const char* path ) {
        unique_lock<shared_mutex> ( opts_lock );
    }

    Type type ( int opt ) {
        return opts[opt].type;
    }

    Type type ( const char* name ) {
        int opt = find ( name );
        return opt == -1 ? Type::None : opts[opt].type;
    }

    int  find ( const char* name ) {
        map<string, int>::iterator res = opts_map.find ( name );
        return res == opts_map.end() ? -1 : res->second;
    }

    bool invalid_type ( int opt, Type type ) {
        if ( opt < 0 || opt >= OPTS_COUNT ) {
            Log::error ( FMT ( "Invalid option %d", opt ) );
            return true;
        }

        if ( type != opts[opt].type ) {
            Log::error ( FMT ( "Invalid type %s for option %s, expected %s", type_str [ ( size_t ) type], opts[opt].name, type_str[ ( size_t ) opts[opt].type] ) );
            return true;
        }

        return false;
    }

    int read_i ( int opt ) {
        shared_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Int ) ) {
            return numeric_limits<int>::quiet_NaN();
        }

        return opts[opt].v.i;
    }

    float read_f ( int opt ) {
        shared_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Real ) ) {
            return numeric_limits<float>::quiet_NaN();
        }

        return opts[opt].v.r;
    }

    string read_s ( int opt ) {
        shared_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Str ) ) {
            return nullptr;
        }

        return opts[opt].v.s;
    }

    void write_i ( int opt, int val ) {
        unique_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Int ) ) {
            return;
        }

        opts[opt].v.i = val;
    }

    void write_f ( int opt, float val ) {
        unique_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Real ) ) {
            return;
        }

        opts[opt].v.r = val;
    }

    void write_s ( int opt, const char* val ) {
        unique_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Real ) ) {
            return;
        }

        if ( opts[opt].v.s == nullptr ) {
            free ( opts[opt].v.s );
        }

        opts[opt].v.s = _strdup ( val );
    }

    bool parse_i ( const char* s, int& v ) {
        char* test = nullptr;
        v = strtol ( s, &test, 10 );

        if ( test != s + strlen ( s ) ) {
            return false;
        }

        return true;
    }

    bool parse_f ( const char* s, float& v ) {
        char* test = nullptr;
        v = ( float ) strtod ( s, &test );

        if ( test != s + strlen ( s ) ) {
            return false;
        }

        return true;
    }
}