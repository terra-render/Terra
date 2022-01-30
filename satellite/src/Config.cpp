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
    #include <Windows.h>
#endif

namespace Config {
    namespace {
        struct Opt {
            Opt ( Type type, const char* name, const char* desc ) :
                type ( type ), name ( name ), desc ( desc ) {
                if ( type == Type::Str || type == Type::Real3 ) {
                    v.s = nullptr;
                }
            }

            Opt() {
                v.s = nullptr;
            }

            ~Opt() {
                if ( type == Type::Str || type == Type::Real3 ) {
                    free ( v.s );
                    v.s = nullptr;
                }
            }

            Opt& operator= ( const Opt& other ) {
                if ( type == Type::Str || type == Type::Real3 ) {
                    free ( v.s );
                    v.s = nullptr;
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
                } else if ( type == Type::Real3 ) {
                    if ( other.v.r3 != nullptr ) {
                        v.r3 = ( float* ) malloc ( sizeof ( float ) * 3 );
                        memcpy ( v.r3, other.v.r3, sizeof ( float ) * 3 );
                    }
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
                float* r3;
            } v;
        };

        const char* type_str[ ( size_t ) Type::Count] {
            "Int", "Real", "String", "Exec"
        };

        // Read only options
        unique_ptr<Opt[]> opts;
        map<string, int>  opts_map;
        shared_mutex      opts_lock;
        const char*       config_path = CONFIG_PATH;

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

        void add_opt ( int idx, float* val, const char* name, const char* desc ) {
            Opt* opt;

            if ( add_opt ( idx, Type::Real3, name, desc, &opt ) ) {
                opt->v.r3 = ( float* ) malloc ( sizeof ( float ) * 3 );
                memcpy ( opt->v.r3, val, sizeof ( float ) * 3 );
            }
        }

        bool file_exists ( const char* path, ifstream& fs ) {
            fs.open ( path );
            return fs.good();
        }

        bool find_config_file ( ifstream& fs ) {
            unique_lock<shared_mutex> ( opts_lock );

            if ( file_exists ( CONFIG_PATH, fs ) ) {
                config_path = CONFIG_PATH;
                return true;
            }

            if ( file_exists ( CONFIG_PATH, fs ) ) {
                config_path = CONFIG_PATH;
                return true;
            }

            if ( file_exists ( "data" CONFIG_PATH, fs ) ) {
                config_path = "data/" CONFIG_PATH;
                return true;
            }

            return false;
        }

        bool next_line ( ifstream& fs, string& name, string& value ) {
            string line;

            // Skip blank lines and comments
            do {
                if ( !fs.good() ) {
                    return false;
                }

                getline ( fs, line );
            } while ( line == "" || line.data() [0] == '#' );

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

    //
    // Terra string => enum mappings
    // they take string& since Config::read_s returns a copy
    // -1 for invalid
    //
#define TRY_COMPARE_S(s, v, ret) if (strcmp((s), (v)) == 0) { return ret; }

    TerraTonemappingOperator to_terra_tonemap ( string& str ) {
        return to_terra_tonemap ( std::move ( str ) );
    }
    TerraTonemappingOperator to_terra_tonemap ( string&& str ) {
        transform ( str.begin(), str.end(), str.begin(), ::tolower );
        const char* s = str.data();
        TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_NONE, kTerraTonemappingOperatorNone );
        TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_LINEAR, kTerraTonemappingOperatorLinear );
        TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_REINHARD, kTerraTonemappingOperatorReinhard );
        TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_FILMIC, kTerraTonemappingOperatorFilmic );
        TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_UNCHARTED2, kTerraTonemappingOperatorUncharted2 );
        return ( TerraTonemappingOperator ) - 1;
    }

    TerraAccelerator to_terra_accelerator ( string& str ) {
        return to_terra_accelerator ( std::move ( str ) );
    }
    TerraAccelerator to_terra_accelerator ( string&& str ) {
        transform ( str.begin(), str.end(), str.begin(), ::tolower );
        const char* s = str.data();
        TRY_COMPARE_S ( s, RENDER_OPT_ACCELERATOR_BVH, kTerraAcceleratorBVH );
        return ( TerraAccelerator ) - 1;
    }

    TerraSamplingMethod to_terra_sampling ( string& str ) {
        return to_terra_sampling ( std::move ( str ) );
    }
    TerraSamplingMethod to_terra_sampling ( string&& str ) {
        transform ( str.begin(), str.end(), str.begin(), ::tolower );
        const char* s = str.data();
        TRY_COMPARE_S ( s, RENDER_OPT_SAMPLER_RANDOM, kTerraSamplingMethodRandom );
        TRY_COMPARE_S ( s, RENDER_OPT_SAMPLER_STRATIFIED, kTerraSamplingMethodStratified );
        TRY_COMPARE_S ( s, RENDER_OPT_SAMPLER_HALTON, kTerraSamplingMethodHalton );
        return ( TerraSamplingMethod ) - 1;
    }

    TerraIntegrator to_terra_integrator ( std::string& str ) {
        return to_terra_integrator ( std::move ( str ) );
    }
    TerraIntegrator to_terra_integrator ( std::string&& str ) {
        transform ( str.begin(), str.end(), str.begin(), ::tolower );
        const char* s = str.data();
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_BASIC, kTerraIntegratorSimple );
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_DIRECT, kTerraIntegratorDirect );
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_DIRECT_MIS, kTerraIntegratorDirectMis );
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_DEBUG_MONO, kTerraIntegratorDebugMono );
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_DEBUG_DEPTH, kTerraIntegratorDebugDepth );
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_DEBUG_NORMALS, kTerraIntegratorDebugNormals );
        TRY_COMPARE_S ( s, RENDER_OPT_INTEGRATOR_DEBUG_MIS, kTerraIntegratorDebugMisWeights );
        return ( TerraIntegrator ) - 1;
    }

    Effect query_change_effect ( int opt ) {
        if ( opt > RENDER_BEGIN && opt < RENDER_END ) {
            return Config::EFFECT_CLEAR;
        }

        return Config::EFFECT_NOOP;
    }

    void _load ( ifstream& fs ) {
        string name, value;

        while ( next_line ( fs, name, value ) ) {
            int opt_idx = find ( name.c_str() );

            if ( opt_idx == -1 ) {
                Log::error ( FMT ( "Unrecognized option name %s", name.c_str() ) );
                continue;
            }

            write ( opt_idx, value );
        }

        //dump();
    }

    bool init ( ) {
        // Lock and allocate opts
        unique_lock<shared_mutex> ( opts_lock );
        opts = unique_ptr<Opt[]> ( new Opt[OPTS_COUNT] );
        // Fill opts
        float campos[] = RENDER_OPT_CAMERA_POS_DEFAULT;
        float camdir[] = RENDER_OPT_CAMERA_DIR_DEFAULT;
        float camup[] = RENDER_OPT_CAMERA_UP_DEFAULT;
        float envmap[] = RENDER_OPT_ENVMAP_COLOR_DEFAULT;
        add_opt ( JOB_N_WORKERS,            RENDER_OPT_WORKERS_DEFAULT,             RENDER_OPT_WORKERS_NAME,            RENDER_OPT_WORKERS_DESC );
        add_opt ( JOB_TILE_SIZE,            RENDER_OPT_TILE_SIZE_DEFAULT,           RENDER_OPT_TILE_SIZE_NAME,          RENDER_OPT_TILE_SIZE_DESC );
        add_opt ( RENDER_MAX_BOUNCES,       RENDER_OPT_BOUNCES_DEFAULT,             RENDER_OPT_BOUNCES_NAME,            RENDER_OPT_BOUNCES_DESC );
        add_opt ( RENDER_SAMPLES,           RENDER_OPT_SAMPLES_DEFAULT,             RENDER_OPT_SAMPLES_NAME,            RENDER_OPT_SAMPLES_DESC );
        add_opt ( RENDER_GAMMA,             RENDER_OPT_GAMMA_DEFAULT,               RENDER_OPT_GAMMA_NAME,              RENDER_OPT_GAMMA_DESC );
        add_opt ( RENDER_EXPOSURE,          RENDER_OPT_EXPOSURE_DEFAULT,            RENDER_OPT_EXPOSURE_NAME,           RENDER_OPT_EXPOSURE_DESC );
        add_opt ( RENDER_TONEMAP,           RENDER_OPT_TONEMAP_DEFAULT,             RENDER_OPT_TONEMAP_NAME,            RENDER_OPT_TONEMAP_DESC );
        add_opt ( RENDER_ACCELERATOR,       RENDER_OPT_ACCELERATOR_DEFAULT,         RENDER_OPT_ACCELERATOR_NAME,        RENDER_OPT_ACCELERATOR_DESC );
        add_opt ( RENDER_SAMPLING,          RENDER_OPT_SAMPLER_DEFAULT,             RENDER_OPT_SAMPLER_NAME,            RENDER_OPT_SAMPLER_DESC );
        add_opt ( RENDER_WIDTH,             RENDER_OPT_WIDTH_DEFAULT,               RENDER_OPT_WIDTH_NAME,              RENDER_OPT_WIDTH_DESC );
        add_opt ( RENDER_HEIGHT,            RENDER_OPT_HEIGHT_DEFAULT,              RENDER_OPT_HEIGHT_NAME,             RENDER_OPT_HEIGHT_DESC );
        add_opt ( VISUALIZER_PROGRESSIVE,   RENDER_OPT_PROGRESSIVE_DEFAULT,         RENDER_OPT_PROGRESSIVE_NAME,        RENDER_OPT_PROGRESSIVE_DESC );
        add_opt ( RENDER_CAMERA_POS,        campos,                                 RENDER_OPT_CAMERA_POS_NAME,         RENDER_OPT_CAMERA_POS_DESC );
        add_opt ( RENDER_CAMERA_DIR,        camdir,                                 RENDER_OPT_CAMERA_DIR_NAME,         RENDER_OPT_CAMERA_DIR_DESC );
        add_opt ( RENDER_CAMERA_UP,         camup,                                  RENDER_OPT_CAMERA_UP_NAME,          RENDER_OPT_CAMERA_UP_DESC );
        add_opt ( RENDER_CAMERA_VFOV_DEG,   RENDER_OPT_CAMERA_VFOV_DEG_DEFAULT,     RENDER_OPT_CAMERA_VFOV_DEG_NAME,    RENDER_OPT_CAMERA_VFOV_DEG_DESC );
        add_opt ( RENDER_SCENE_PATH,        RENDER_OPT_SCENE_PATH_DEFAULT,          RENDER_OPT_SCENE_PATH_NAME,         RENDER_OPT_SCENE_PATH_DESC );
        add_opt ( RENDER_ENVMAP_COLOR,      envmap,                                 RENDER_OPT_ENVMAP_COLOR_NAME,       RENDER_OPT_ENVMAP_COLOR_DESC );
        add_opt ( RENDER_JITTER,            RENDER_OPT_JITTER_DEFAULT,              RENDER_OPT_JITTER_NAME,             RENDER_OPT_JITTER_DESC );
        add_opt ( RENDER_INTEGRATOR,        RENDER_OPT_INTEGRATOR_DEFAULT,          RENDER_OPT_INTEGRATOR_NAME,         RENDER_OPT_INTEGRATOR_DESC );
        /*if ( !load () ) {
            Log::info ( STR ( "No configuration file loaded." ) );
            return true;
        }*/
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

                case Type::Real3:
                    Log::info ( FMT ( "%s = %f %f %f", opts[i].name, opts[i].v.r3[0], opts[i].v.r3[1], opts[i].v.r3[2] ) );
                    break;

                default:
                    break;
            }
        }
    }

    void dump_desc ( int from, int to ) {
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
            char buffer[128];

            switch ( opts[i].type ) {
                case Type::Int:
                    sprintf ( buffer, "%s = %d", opts[i].name, opts[i].v.i );
                    break;

                case Type::Real:
                    sprintf ( buffer, "%s = %f", opts[i].name, opts[i].v.r );
                    break;

                case Type::Str:
                    sprintf ( buffer, "%s = %s", opts[i].name, opts[i].v.s );
                    break;

                case Type::Real3:
                    sprintf ( buffer, "%s = %f %f %f", opts[i].name, opts[i].v.r3[0], opts[i].v.r3[1], opts[i].v.r3[2] );
                    break;

                default:
                    break;
            }

            Log::console ( "%-30s : %s", buffer, opts[i].desc );
        }
    }

    void reset_to_default() {
        int n_threads = ( int ) thread::hardware_concurrency();
        float campos[] = RENDER_OPT_CAMERA_POS_DEFAULT;
        float camdir[] = RENDER_OPT_CAMERA_DIR_DEFAULT;
        float camup[] = RENDER_OPT_CAMERA_UP_DEFAULT;
        float envmap[] = RENDER_OPT_ENVMAP_COLOR_DEFAULT;
        write_i ( JOB_N_WORKERS, n_threads );
        write_i ( JOB_TILE_SIZE, RENDER_OPT_TILE_SIZE_DEFAULT );
        write_i ( RENDER_MAX_BOUNCES, RENDER_OPT_BOUNCES_DEFAULT );
        write_i ( RENDER_SAMPLES, RENDER_OPT_SAMPLES_DEFAULT );
        write_f ( RENDER_GAMMA, RENDER_OPT_GAMMA_DEFAULT );
        write_f ( RENDER_EXPOSURE, RENDER_OPT_EXPOSURE_DEFAULT );
        write_s ( RENDER_TONEMAP, RENDER_OPT_TONEMAP_DEFAULT );
        write_s ( RENDER_ACCELERATOR, RENDER_OPT_ACCELERATOR_DEFAULT );
        write_s ( RENDER_SAMPLING, RENDER_OPT_SAMPLER_DEFAULT );
        write_i ( RENDER_WIDTH, RENDER_OPT_WIDTH_DEFAULT );
        write_i ( RENDER_HEIGHT, RENDER_OPT_HEIGHT_DEFAULT );
        write_i ( VISUALIZER_PROGRESSIVE, RENDER_OPT_PROGRESSIVE_DEFAULT );
        write_f3 ( RENDER_CAMERA_POS, campos );
        write_f3 ( RENDER_CAMERA_DIR, camdir );
        write_f3 ( RENDER_CAMERA_UP, camup );
        write_f ( RENDER_CAMERA_VFOV_DEG, RENDER_OPT_CAMERA_VFOV_DEG_DEFAULT );
        write_s ( RENDER_SCENE_PATH, RENDER_OPT_SCENE_PATH_DEFAULT );
        write_f3 ( RENDER_ENVMAP_COLOR, envmap );
        write_f ( RENDER_JITTER, RENDER_OPT_JITTER_DEFAULT );
        write_s ( RENDER_INTEGRATOR, RENDER_OPT_INTEGRATOR_DEFAULT );
    }

    bool load ( const char* path ) {
        unique_lock<shared_mutex> ( opts_lock );
        ifstream fs;

        if ( !file_exists ( path, fs ) ) {
            return false;
        }

        _load ( fs );
        config_path = path;
        return true;
    }

    bool load() {
        unique_lock<shared_mutex> ( opts_lock );
        ifstream fs;

        if ( !find_config_file ( fs ) ) {
            return false;
        }

        _load ( fs );
        return true;
    }

    bool save ( const char* path ) {
        unique_lock<shared_mutex> ( opts_lock );
        // TODO save at given path
        Log::info ( FMT ( "Feature not supported yet." ) );
        return true;
    }

    bool save() {
        unique_lock<shared_mutex> ( opts_lock );
        // TODO save at config_path
        Log::info ( FMT ( "Feature not supported yet." ) );
        return true;
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

    TerraFloat3 read_f3 ( int opt ) {
        shared_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Real3 ) ) {
            return terra_f3_set1 ( numeric_limits<float>::quiet_NaN() );
        }

        return terra_f3_set ( opts[opt].v.r3[0], opts[opt].v.r3[1], opts[opt].v.r3[2] );
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

    void write_f3 ( int opt, float* val ) {
        unique_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Real ) ) {
            return;
        }

        memcpy ( opts[opt].v.r3, val, sizeof ( float ) * 3 );
    }

    void write_s ( int opt, const char* val ) {
        unique_lock<shared_mutex> ( opts_lock );

        if ( invalid_type ( opt, Type::Str ) ) {
            return;
        }

        if ( opts[opt].v.s == nullptr ) {
            free ( opts[opt].v.s );
        }

        opts[opt].v.s = _strdup ( val );
    }

    void write ( int opt_idx, const std::string& value ) {
        unique_lock<shared_mutex> ( opts_lock );
        Opt& opt = opts[opt_idx];

        switch ( opt.type ) {
            case Type::Int: {
                int v;

                if ( !parse_i ( value.c_str(), v ) ) {
                    Log::error ( FMT ( "Argument %s expects an integral, but found %s", value.c_str() ) );
                } else {
                    opt.v.i = v;
                }

                break;
            }

            case Type::Real: {
                float v;

                if ( !parse_f ( value.c_str(), v ) ) {
                    Log::error ( FMT ( "Argument %s expects a real number, but found %s", value.c_str() ) );
                } else {
                    opt.v.r = v;
                }

                break;
            }

            case Type::Real3: {
                float v[3];

                if ( !parse_f3 ( value.c_str(), v ) ) {
                    Log::error ( FMT ( "Argument %s expects a 3d real vector, but found %s", value.c_str() ) );
                } else {
                    memcpy ( opt.v.r3, v, sizeof ( float ) * 3 );
                }

                break;
            }

            case Type::Str: {
                opt.v.s = _strdup ( value.c_str() );
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

    bool parse_f3 ( const char* s, float* v ) {
        char* test = nullptr;
        v[0] = ( float ) strtod ( s, &test );
        v[1] = ( float ) strtod ( test, &test );
        v[2] = ( float ) strtod ( test, &test );

        if ( test != s + strlen ( s ) ) {
            return false;
        }

        return true;
    }
}