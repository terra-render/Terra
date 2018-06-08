// Header
#include <Scene.hpp>

// stdlib
#include <cstdio>
#include <utility>
#include <algorithm>

// Satellite
#include <Logging.hpp>
#include <Config.hpp>

// Terra
#include <TerraPresets.h>

// Apollo
#define APOLLO_IMPLEMENTATION
#include <Apollo.h>

using namespace std;

namespace {
    constexpr float       CAMERA_FOV = 60.f;
    constexpr TerraFloat3 CAMERA_POS = { 0.f, 0.7f, 2.f };
    constexpr TerraFloat3 CAMERA_DIR = { 0.f, 0.f, -1.f };
    constexpr TerraFloat3 CAMERA_UP  = { 0.f, 1.f, 0.f };

    constexpr TerraFloat3 ENVMAP_COLOR = { 0.4f, 0.52f, 1.f };

    // Making life easier, more readability, less errors (hpf)
#define READ_ATTR(attr, apollo_attr, textures )\
    {\
        int tidx = apollo_attr##_map_id;\
        TerraTexture* texture = nullptr;\
        TerraFloat3   constant = to_constant(apollo_attr);\
        if (tidx != -1) { texture = _allocate_texture(textures[tidx].name); }\
        if (texture == nullptr) { terra_attribute_init_constant(&attr, &constant); }\
        else { terra_attribute_init_texture(&attr, texture);}}

    template <typename T>
    TerraFloat3 to_constant ( const T& v ) { }

    template <>
    TerraFloat3 to_constant ( const ApolloFloat3& v ) {
        return terra_f3_set ( v.x, v.y, v.z );
    }

    template <>
    TerraFloat3 to_constant ( const float& v ) {
        return terra_f3_set1 ( v );
    }
}

//
// Terra string => enum mappings
// they take string& since Config::read_s returns a copy
// -1 for invalid
//
#define TRY_COMPARE_S(s, v, ret) if (strcmp((s), (v)) == 0) { return ret; }
TerraTonemappingOperator Scene::to_terra_tonemap ( string& str ) {
    transform ( str.begin(), str.end(), str.begin(), ::tolower );
    const char* s = str.data();
    TRY_COMPARE_S ( s, "none", kTerraTonemappingOperatorNone );
    TRY_COMPARE_S ( s, "linear", kTerraTonemappingOperatorLinear );
    TRY_COMPARE_S ( s, "reinhard", kTerraTonemappingOperatorReinhard );
    TRY_COMPARE_S ( s, "filmic", kTerraTonemappingOperatorFilmic );
    TRY_COMPARE_S ( s, "uncharted2", kTerraTonemappingOperatorUncharted2 );
    return ( TerraTonemappingOperator ) - 1;
}

TerraAccelerator Scene::to_terra_accelerator ( string& str ) {
    transform ( str.begin(), str.end(), str.begin(), ::tolower );
    const char* s = str.data();
    TRY_COMPARE_S ( s, "bvh", kTerraAcceleratorBVH );
    TRY_COMPARE_S ( s, "kdtree", kTerraAcceleratorKDTree );
    return ( TerraAccelerator ) - 1;
}

TerraSamplingMethod Scene::to_terra_sampling ( string& str ) {
    transform ( str.begin(), str.end(), str.begin(), ::tolower );
    const char* s = str.data();
    TRY_COMPARE_S ( s, "random", kTerraSamplingMethodRandom );
    TRY_COMPARE_S ( s, "stratified", kTerraSamplingMethodStratified );
    TRY_COMPARE_S ( s, "halton", kTerraSamplingMethodHalton );
    return ( TerraSamplingMethod ) - 1;
}

const char* Scene::from_terra_tonemap ( TerraTonemappingOperator v ) {
    const char* names[] = {
        "none",
        "linear",
        "reinhard",
        "filmic",
        "uncharted2"
    };
    int idx = ( int ) v - ( int ) kTerraTonemappingOperatorNone;

    if ( idx >= 0 && idx < 5 ) {
        return names[idx];
    }

    return nullptr;
}

const char* Scene::from_terra_accelerator ( TerraAccelerator v ) {
    const char* names[] = {
        "bvh",
        "kdtree"
    };
    int idx = ( int ) v - ( int ) kTerraAcceleratorBVH;

    if ( idx >= 0 && idx < 2 ) {
        return names[idx];
    }

    return nullptr;
}

const char* Scene::from_terra_sampling ( TerraSamplingMethod v ) {
    const char* names[] = {
        "random",
        "stratified",
        "halton"
    };
    int idx = ( int ) v - ( int ) kTerraSamplingMethodRandom;

    if ( idx >= 0 && idx < 3 ) {
        return names[idx];
    }

    return nullptr;
}

Scene::Scene() {
    memset ( &_scene, 0, sizeof ( HTerraScene ) );
}

Scene::~Scene() {
}

bool Scene::load ( const char* filename ) {
    // TODO move this into Scene constructor?
    if ( _first_load ) {
        reset_options();
    }

    clear(); // release current scene

    if ( filename == nullptr ) {
        return false;
    }

    ApolloMaterial* materials = NULL;
    ApolloTexture* textures = NULL;
    ApolloModel model;

    if ( apollo_import_model_obj ( filename, &model, &materials, &textures, false, false ) != APOLLO_SUCCESS ) {
        Log::error ( "Failed to import %s", filename );
        return false;
    }

    // Extracing filename as name
    size_t filename_len = strlen ( filename );
    const char* name = filename + filename_len - 1;

    while ( name != filename && ( *name != '/' && *name != '\\' ) ) {
        --name;
    }

    name = name != '\0' ? name + 1 : name;
    _name = name;
    //
    // Importing into Terra Scene
    //
    Log::info ( FMT ( "Read %d objects from %s", model.mesh_count, name ) );
    _scene = terra_scene_create();
    uint64_t n_triangles = 0;

    for ( int m = 0; m < model.mesh_count; ++m ) {
        TerraObject* object = terra_scene_add_object ( _scene, model.meshes[m].face_count );
        //
        // Reading geometry
        //

        for ( int i = 0; i < object->triangles_count; ++i ) {
            const ApolloMeshFace* apollo_face = &model.meshes[m].faces[i];
            object->triangles[i].b = * ( TerraFloat3* ) &model.vertices[apollo_face->b].pos;
            object->triangles[i].a = * ( TerraFloat3* ) &model.vertices[apollo_face->a].pos;
            object->triangles[i].c = * ( TerraFloat3* ) &model.vertices[apollo_face->c].pos;
            object->properties[i].normal_a = * ( TerraFloat3* ) &model.vertices[apollo_face->a].norm;
            object->properties[i].normal_b = * ( TerraFloat3* ) &model.vertices[apollo_face->b].norm;
            object->properties[i].normal_c = * ( TerraFloat3* ) &model.vertices[apollo_face->c].norm;
            object->properties[i].texcoord_a = * ( TerraFloat2* ) &model.vertices[apollo_face->a].tex;
            object->properties[i].texcoord_b = * ( TerraFloat2* ) &model.vertices[apollo_face->b].tex;
            object->properties[i].texcoord_c = * ( TerraFloat2* ) &model.vertices[apollo_face->c].tex;
        }

        //
        // Reading materials
        //
        const int material_idx = model.meshes[m].material_id;
        const ApolloMaterial& material = materials[material_idx];
        object->material.ior = 1.5;
        // TODO
        object->material.enable_bump_map_attr = 0;
        object->material.enable_normal_map_attr = 0;
        READ_ATTR ( object->material.emissive, material.emissive, textures );

        switch ( material.bsdf ) {
            case APOLLO_SPECULAR: {
                TerraAttribute albedo, specular_color, specular_intensity;
                READ_ATTR ( albedo, material.diffuse, textures );
                READ_ATTR ( specular_color, material.specular, textures );
                READ_ATTR ( specular_intensity, material.specular_exp, textures );
                object->material.attributes[TERRA_PHONG_ALBEDO]             = albedo;
                object->material.attributes[TERRA_PHONG_SPECULAR_COLOR]     = specular_color;
                object->material.attributes[TERRA_PHONG_SPECULAR_INTENSITY] = specular_intensity;
                object->material.attributes_count                           = TERRA_PHONG_END;
                terra_bsdf_phong_init ( &object->material.bsdf );
                break;
            }

            default:
            case APOLLO_MIRROR:
                Log::warning ( FMT ( "Scene(%s) Unsupported mirror material(%s). Defaulting to diffuse." ), name, material.name );

            case APOLLO_PBR:
                Log::warning ( FMT ( "Scene(%s) Unsupported pbr material(%s). Defaulting to diffuse" ), name, material.name );

            case APOLLO_DIFFUSE: {
                TerraAttribute albedo;
                READ_ATTR ( albedo, material.diffuse, textures );
                object->material.attributes[TERRA_DIFFUSE_ALBEDO] = albedo;
                object->material.attributes_count                 = TERRA_DIFFUSE_END;
                terra_bsdf_diffuse_init ( &object->material.bsdf );
                break;
            }
        }

        n_triangles += object->triangles_count;
    }

    // TODO free materials/textures?
    Log::info ( FMT ( "Building acceleration structure for %zu triangles", n_triangles ) );
    terra_scene_commit ( _scene );
    Log::info ( FMT ( "Finished importing %s. objects(%d) textures(%d)", name, terra_scene_count_objects ( _scene ), _textures.size() ) );
    return true;
}

void Scene::clear() {
    if ( _scene != NULL ) {
        terra_scene_clear ( _scene );
    }

    _textures.clear();
    memset ( &_scene, 0, sizeof ( HTerraScene ) );
}

void Scene::reset_options() {
    string tonemap_str     = Config::read_s ( Config::RENDER_TONEMAP );
    string accelerator_str = Config::read_s ( Config::RENDER_ACCELERATOR );
    string sampling_str    = Config::read_s ( Config::RENDER_SAMPLING );
    TerraTonemappingOperator tonemap = Scene::to_terra_tonemap ( tonemap_str );
    TerraAccelerator accelerator     = Scene::to_terra_accelerator ( accelerator_str );
    TerraSamplingMethod sampling     = Scene::to_terra_sampling ( sampling_str );

    if ( tonemap == -1 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_TONEMAP value %s. Defaulting to none.", tonemap_str.c_str() ) );
        tonemap = kTerraTonemappingOperatorNone;
    }

    if ( accelerator == -1 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_ACCELERATOR value %s. Defaulting to BVH.", accelerator_str.c_str() ) );
        accelerator = kTerraAcceleratorBVH;
    }

    if ( sampling == -1 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_SAMPLING value %s. Defaulting to random.", sampling_str.c_str() ) );
        sampling = kTerraSamplingMethodRandom;
    }

    int bounces = Config::read_i ( Config::RENDER_MAX_BOUNCES );
    int samples = Config::read_i ( Config::RENDER_SAMPLES );
    float exposure = Config::read_f ( Config::RENDER_EXPOSURE );
    float gamma = Config::read_f ( Config::RENDER_GAMMA );

    if ( bounces < 0 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_MAX_BOUNCES (%d < 0). Defaulting to 64.", bounces ) );
        bounces = 64;
    }

    if ( samples < 0 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_SAMPLES (%d < 0). Defaulting to 8.", samples ) );
        samples = 8;
    }

    if ( exposure < 0 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_EXPOSURE (%f < 0). Defaulting to 1.0.", exposure ) );
        exposure = 1.0;
    }

    if ( gamma < 0 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_GAMMA (%f < 0). Defaulting to 2.2.", gamma ) );
        gamma = 2.2f;
    }

    _opts.bounces              = bounces;
    _opts.samples_per_pixel    = samples;
    _opts.subpixel_jitter      = 0.f; // Please remove this option and implement proper antialiasing
    _opts.tonemapping_operator = tonemap;
    _opts.manual_exposure      = exposure;
    _opts.gamma                = gamma;
    _opts.accelerator          = accelerator;
    _opts.direct_sampling      = false;
    _opts.strata               = 4;
    _opts.sampling_method      = sampling;
    terra_attribute_init_constant ( &_opts.environment_map, &ENVMAP_COLOR );
    _default_camera.fov       = CAMERA_FOV;
    _default_camera.position  = CAMERA_POS;
    _default_camera.direction = CAMERA_DIR;
    _default_camera.up        = CAMERA_UP;
}

void Scene::apply_options_to_config() {
    Config::write_i ( Config::RENDER_MAX_BOUNCES, _opts.bounces );
    Config::write_i ( Config::RENDER_SAMPLES, _opts.samples_per_pixel );
    Config::write_s ( Config::RENDER_TONEMAP, from_terra_tonemap ( _opts.tonemapping_operator ) );
    Config::write_f ( Config::RENDER_EXPOSURE, _opts.manual_exposure );
    Config::write_f ( Config::RENDER_GAMMA, _opts.gamma );
    Config::write_s ( Config::RENDER_ACCELERATOR, from_terra_accelerator ( _opts.accelerator ) );
    Config::write_s ( Config::RENDER_SAMPLING, from_terra_sampling ( _opts.sampling_method ) );
}

bool Scene::set_opt ( int opt, const char* value ) {
    Config::Type type = Config::type ( opt );

    if ( type == Config::Type::Int ) {
        int i;

        if ( Config::parse_i ( value, i ) ) {
            return _set_opt_safe ( opt, &i );
        } else {
            Log::error ( FMT ( "Value %s cannot be converted to int for option %d", value, opt ) );
        }
    } else if ( type == Config::Type::Real ) {
        float f;

        if ( Config::parse_f ( value, f ) ) {
            return _set_opt_safe ( opt, &f );
        } else {
            Log::error ( FMT ( "Value %s cannot be converted to int for option %d", value, opt ) );
        }
    } else if ( type == Config::Type::Str ) {
        return _set_opt_safe ( opt, value );
    }

    return false;
}

void Scene::dump_opts() {
    Log::info ( FMT ( "Samples per pixel = %d", _opts.samples_per_pixel ) );
    Log::info ( FMT ( "Accelerator       = %s", Scene::from_terra_accelerator ( _opts.accelerator ) ) );
    Log::info ( FMT ( "Sampling          = %s", Scene::from_terra_sampling ( _opts.sampling_method ) ) );
}

HTerraScene Scene::construct_terra_scene() {
    TerraSceneOptions* opts = terra_scene_get_options ( _scene );
    *opts = _opts;
    terra_scene_commit ( _scene );
    return _scene;
}

const char* Scene::name() const {
    return _name.c_str();
}

TerraCamera Scene::default_camera() {
    return _default_camera;
}

bool Scene::_set_opt_safe ( int opt, const void* data ) {
    assert ( data != nullptr );

    switch ( opt ) {
        case Config::RENDER_MAX_BOUNCES:
            _opts.bounces = * ( int* ) data;
            break;

        case Config::RENDER_SAMPLES:
            _opts.samples_per_pixel = * ( int* ) data;
            break;

        case Config::RENDER_GAMMA:
            _opts.gamma = * ( float* ) data;
            break;

        case Config::RENDER_EXPOSURE:
            _opts.manual_exposure = * ( float* ) data;
            break;

        case Config::RENDER_TONEMAP: {
            string s = ( const char* ) data;
            TerraTonemappingOperator v = to_terra_tonemap ( s );

            if ( v != -1 ) {
                _opts.tonemapping_operator = v;
            } else {
                Log::error ( FMT ( "Invalid value %s for RENDER_TONEMAP", data ) );
                return false;
            }

            break;
        }

        case Config::RENDER_ACCELERATOR: {
            string s = ( const char* ) data;
            TerraAccelerator v = to_terra_accelerator ( s );

            if ( v != -1 ) {
                _opts.accelerator = v;
            } else {
                Log::error ( FMT ( "Invalid value %s for RENDER_ACCELERATOR", data ) );
                return false;
            }

            break;
        }

        case Config::RENDER_SAMPLING: {
            string s = ( const char* ) data;
            TerraSamplingMethod v = to_terra_sampling ( s );

            if ( v != -1 ) {
                _opts.sampling_method = v;
            } else {
                Log::error ( FMT ( "Invalid value %s for RENDER_SAMPLING", data ) );
                return false;
            }

            break;
        }

        default:
            return false;
    }

    return true;
}

TerraTexture* Scene::_allocate_texture ( const char* path ) {
    return nullptr;
}