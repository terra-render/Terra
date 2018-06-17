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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define APOLLO_IMPLEMENTATION
#include <Apollo.h>

using namespace std;

namespace {
    constexpr float       CAMERA_FOV = 60.f;
    constexpr TerraFloat3 CAMERA_POS = { 2.f, 2.f, 2.f };
    constexpr TerraFloat3 CAMERA_DIR = { -1.f, -1.f, -1.f };
    constexpr TerraFloat3 CAMERA_UP  = { 0.f, 1.f, 0.f };

    constexpr TerraFloat3 ENVMAP_COLOR = { 0.4f, 0.52f, 1.f };

    // Making life easier, more readability, less errors (hpf)
#define READ_ATTR(attr, apollo_attr, textures )\
    {\
        int tidx = apollo_attr##_map_id;\
        TerraTexture* texture = nullptr;\
        TerraFloat3   constant = to_constant(apollo_attr);\
        if (tidx != -1) { texture = _allocate_texture(_textures + tidx); }\
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

Scene::Scene() :
    _materials ( nullptr ),
    _textures ( nullptr ) {
    memset ( &_scene, 0, sizeof ( HTerraScene ) );
    memset ( &_models, 0, sizeof ( ApolloModel ) );
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

    if ( apollo_import_model_obj ( filename, &_models, &_materials, &_textures, false, false ) != APOLLO_SUCCESS ) {
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
    Log::info ( FMT ( "Read %d objects from %s", _models.mesh_count, name ) );
    _scene = terra_scene_create();
    uint64_t n_triangles = 0;

    for ( size_t m = 0; m < _models.mesh_count; ++m ) {
        size_t object_idx = terra_scene_count_objects ( _scene );
        TerraObject* object = terra_scene_add_object ( _scene, _models.meshes[m].face_count );

        //
        // Reading geometry
        //

        for ( size_t i = 0; i < object->triangles_count; ++i ) {
            const ApolloMeshFace* apollo_face = &_models.meshes[m].faces[i];
            object->triangles[i].b           = * ( TerraFloat3* ) &_models.vertices[apollo_face->b].pos;
            object->triangles[i].a           = * ( TerraFloat3* ) &_models.vertices[apollo_face->a].pos;
            object->triangles[i].c           = * ( TerraFloat3* ) &_models.vertices[apollo_face->c].pos;
            object->properties[i].normal_a   = * ( TerraFloat3* ) &_models.vertices[apollo_face->a].norm;
            object->properties[i].normal_b   = * ( TerraFloat3* ) &_models.vertices[apollo_face->b].norm;
            object->properties[i].normal_c   = * ( TerraFloat3* ) &_models.vertices[apollo_face->c].norm;
            object->properties[i].texcoord_a = * ( TerraFloat2* ) &_models.vertices[apollo_face->a].tex;
            object->properties[i].texcoord_b = * ( TerraFloat2* ) &_models.vertices[apollo_face->b].tex;
            object->properties[i].texcoord_c = * ( TerraFloat2* ) &_models.vertices[apollo_face->c].tex;
        }

        //
        // Reading materials
        //
        const int material_idx = _models.meshes[m].material_id;
        const ApolloMaterial& material = _materials[material_idx];

        _material_map[object_idx] = material_idx;

        terra_material_init ( &object->material );

        // IOR
        TerraFloat3 ior = terra_f3_set1 ( 1.5f );
        TerraAttribute ior_attr;
        terra_attribute_init_constant ( &ior_attr, &ior );
        terra_material_ior ( &object->material, &ior_attr );

        // Emissive
        TerraAttribute emissive_attr;
        READ_ATTR ( emissive_attr, material.emissive, textures );
        terra_material_emissive ( &object->material, &emissive_attr );

        // Bump mapping TODO
        if ( material.bump_map_id != APOLLO_INVALID_TEXTURE ) {

        }

        // Normal mapping TODO
        if ( material.normal_map_id != APOLLO_INVALID_TEXTURE ) {

        }

        switch ( material.bsdf ) {
#if 0

            case APOLLO_SPECULAR: {
                TerraAttribute albedo, specular_color, specular_intensity;
                READ_ATTR ( albedo, material.diffuse, textures );
                READ_ATTR ( specular_color, material.specular, textures );
                READ_ATTR ( specular_intensity, material.specular_exp, textures );
                object->material.bsdf_attrs[TERRA_PHONG_ALBEDO]             = albedo;
                object->material.bsdf_attrs[TERRA_PHONG_SPECULAR_COLOR]     = specular_color;
                object->material.bsdf_attrs[TERRA_PHONG_SPECULAR_INTENSITY] = specular_intensity;
                terra_bsdf_phong_init ( &object->material.bsdf );
                break;
            }

#endif

            default:
            case APOLLO_MIRROR:
                Log::warning ( FMT ( "Scene(%s) Unsupported mirror material(%s). Defaulting to diffuse." ), name, material.name );

            case APOLLO_PBR:
                Log::warning ( FMT ( "Scene(%s) Unsupported pbr material(%s). Defaulting to diffuse" ), name, material.name );

            case APOLLO_DIFFUSE: {
                TerraAttribute albedo;
                READ_ATTR ( albedo, material.diffuse, textures );
                object->material.bsdf_attrs[TERRA_DIFFUSE_ALBEDO] = albedo;
                terra_bsdf_diffuse_init ( &object->material.bsdf );
                break;
            }
        }

        n_triangles += object->triangles_count;
    }

    Log::info ( FMT ( "Building acceleration structure for %zu triangles", n_triangles ) );
    terra_scene_commit ( _scene );
    Log::info ( FMT ( "Finished importing %s. objects(%d) textures(%d)", name, terra_scene_count_objects ( _scene ), apollo_buffer_size ( _textures ) ) );
    return true;
}

void Scene::clear() {
    for ( TerraTexture& texture : _terra_textures ) {
        terra_texture_destroy ( &texture );
    }

    _terra_textures.clear();

    if ( _scene != NULL ) {
        terra_scene_destroy ( _scene );
    }

    apollo_textures_free ( _textures );
    apollo_materials_free ( _materials );
    apollo_model_free ( &_models );
    memset ( &_scene, 0, sizeof ( HTerraScene ) );

    _material_map.clear();
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

std::string Scene::material_find ( TerraPrimitiveRef ref ) {
    ApolloMaterial* mat = _find_material ( ref );
    return mat ? mat->name : "";
}

void Scene::material_attr_set ( const char* name, const TerraFloat3* constant ) {

}

void Scene::material_attr_set ( const char* name, const char* path ) {

}

ApolloMaterial* Scene::_find_material ( TerraPrimitiveRef ref ) {
    auto res = _material_map.find ( ref.object_idx );

    if ( res == _material_map.end() ) {
        Log::error ( FMT ( "Invalid primitive ref %d:%d", ref.object_idx, ref.triangle_idx ) );
        return nullptr;
    }

    return _materials + res->second;
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

TerraTexture* Scene::_allocate_texture ( ApolloTexture* apollo_texture ) {
    TerraTexture texture;

    int sampler                 = kTerraSamplerBilinear;
    TerraTextureAddress address = kTerraTextureAddressWrap;

    bool ret = false;

    if ( apollo_texture->type == APOLLO_TEXTURE_TYPE_UINT8 ) {
        ret = terra_texture_create ( &texture, ( uint8_t* ) apollo_texture->data, apollo_texture->width, apollo_texture->height, apollo_texture->channels, sampler, address );
    } else if ( apollo_texture->type == APOLLO_TEXTURE_TYPE_FLOAT32 ) {
        ret = terra_texture_create_fp ( &texture, ( float* ) apollo_texture->data, apollo_texture->width, apollo_texture->height, apollo_texture->channels, sampler, address );
    } else {
        Log::error ( FMT ( "Invalid type %d texture %s", apollo_texture->type, apollo_texture->path ) );
        return nullptr;
    }

    if ( !ret ) {
        Log::error ( FMT ( "Failed to create texture %dx%d %s", apollo_texture->width, apollo_texture->height, apollo_texture->path ) );
        return nullptr;
    }

    _terra_textures.emplace_back ( std::move ( texture ) );
    return &_terra_textures.back();
}
