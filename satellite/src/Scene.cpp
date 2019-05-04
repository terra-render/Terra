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
    constexpr float       CAMERA_FOV = 45.f;
    constexpr TerraFloat3 CAMERA_POS = { 0.f, 0.9f, 2.3f };
    constexpr TerraFloat3 CAMERA_DIR = { 0.f, 0.f, 1.f };
    constexpr TerraFloat3 CAMERA_UP  = { 0.f, 1.f, 0.f };

    constexpr TerraFloat3 ENVMAP_COLOR = { 0.4f, 0.52f, 1.f };

    // Making life easier, more readability, less errors (hpf)
#define READ_ATTR(attr, apollo_attr, textures )\
    {\
        int tidx = apollo_attr##_texture;\
        TerraTexture* texture = nullptr;\
        TerraFloat3   constant = to_constant(apollo_attr);\
        if (tidx != -1) { texture = _allocate_texture(textures[tidx].name); }\
        if (texture == nullptr) { terra_attribute_init_constant(&attr, &constant); }\
        else { terra_attribute_init_texture(&attr, texture);}}

    template <typename T>
    TerraFloat3 to_constant ( const T v ) { }

    template <typename T>
    TerraFloat3 to_constant ( const T* v ) { }

    template <>
    TerraFloat3 to_constant ( const float* v ) {
        return terra_f3_set ( v[0], v[1], v[2] );
    }

    template <>
    TerraFloat3 to_constant ( const float v ) {
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
    TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_NONE, kTerraTonemappingOperatorNone );
    TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_LINEAR, kTerraTonemappingOperatorLinear );
    TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_REINHARD, kTerraTonemappingOperatorReinhard );
    TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_FILMIC, kTerraTonemappingOperatorFilmic );
    TRY_COMPARE_S ( s, RENDER_OPT_TONEMAP_UNCHARTED2, kTerraTonemappingOperatorUncharted2 );
    return ( TerraTonemappingOperator ) - 1;
}

TerraAccelerator Scene::to_terra_accelerator ( string& str ) {
    transform ( str.begin(), str.end(), str.begin(), ::tolower );
    const char* s = str.data();
    TRY_COMPARE_S ( s, "bvh", kTerraAcceleratorBVH );
    return ( TerraAccelerator ) - 1;
}

TerraSamplingMethod Scene::to_terra_sampling ( string& str ) {
    transform ( str.begin(), str.end(), str.begin(), ::tolower );
    const char* s = str.data();
    TRY_COMPARE_S ( s, RENDER_OPT_SAMPLER_RANDOM, kTerraSamplingMethodRandom );
    TRY_COMPARE_S ( s, RENDER_OPT_SAMPLER_STRATIFIED, kTerraSamplingMethodStratified );
    TRY_COMPARE_S ( s, RENDER_OPT_SAMPLER_HALTON, kTerraSamplingMethodHalton );
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

// Just use malloc for everything
void* apollo_alloc ( void* _, size_t size, size_t align ) {
    return malloc ( size );
}
void* apollo_realloc ( void* _, void* p, size_t old_size, size_t new_size, size_t align ) {
    return realloc ( p, new_size );
}
void apollo_free ( void* _, void* p, size_t size ) {
    free ( p );
}

bool Scene::_load_scene ( const char* filename ) {
    if ( filename == nullptr ) {
        return false;
    }

    free ( _apollo_model );
    _apollo_model = ( ApolloModel* ) malloc ( sizeof ( ApolloModel ) );
    ApolloLoadOptions options = { 0 };
    options.compute_tangents = true;
    options.compute_bitangents = true;
    options.compute_bounds = true;
    options.compute_face_normals = true;
    options.recompute_vertex_normals = true;
    options.remove_vertex_duplicates = true;
    options.flip_faces = false;
    options.flip_texcoord_v = false;
    options.flip_z = false;
    options.temp_alloc = &apollo_alloc;
    options.temp_realloc = &apollo_realloc;
    options.temp_free = &apollo_free;
    options.final_alloc = &apollo_alloc;
    options.prealloc_vertex_count = 1 << 18;
    options.prealloc_index_count = 1 << 18;
    options.prealloc_mesh_count = 16;
    Log::info ( FMT ( "Importing model file %s", filename ) );

    if ( apollo_import_model_obj ( filename, _apollo_model, &_apollo_materials, &_apollo_textures, &options ) != APOLLO_SUCCESS ) {
        Log::error ( "Failed to import %s", filename );
        return false;
    }

    int bsdf_count[APOLLO_BSDF_COUNT];
    memset ( bsdf_count, 0, sizeof ( int ) * APOLLO_BSDF_COUNT );

    for ( size_t i = 0; i < sb_count ( _apollo_materials ); ++i ) {
        bsdf_count[_apollo_materials[i].bsdf] += 1;
    }

    Log::info ( FMT ( "Finished importing %s.", filename ) );
    Log::info ( FMT ( "Meshes(%d) [ pbr(%d) diffuse(%d) specular(%d) mirror(%d) ], textures(%d)",
                      _apollo_model->mesh_count, bsdf_count[APOLLO_PBR], bsdf_count[APOLLO_DIFFUSE], bsdf_count[APOLLO_SPECULAR], bsdf_count[APOLLO_MIRROR], _textures.size() ) );
    return true;
}

bool Scene::_build_scene() {
    // TODO move this into Scene constructor?
    if ( _first_load ) {
        reset_options();
    }

    clear(); // release current scene
    //
    // Importing into Terra Scene
    //
    Log::info ( FMT ( "Building %d meshes from %s", _apollo_model->mesh_count, _apollo_model->name ) );
    _scene = terra_scene_create();
    uint64_t n_triangles = 0;

    for ( int m = 0; m < _apollo_model->mesh_count; ++m ) {
        TerraObject* object = terra_scene_add_object ( _scene, _apollo_model->meshes[m].face_count );
        //
        // Reading geometry
        //

        for ( size_t i = 0; i < object->triangles_count; ++i ) {
            const ApolloMeshFaceData* face = &_apollo_model->meshes[m].face_data;
            const ApolloModelVertexData* vertex_data = &_apollo_model->vertex_data;
            object->triangles[i].a.x = vertex_data->pos_x[face->idx_a[i]];
            object->triangles[i].a.y = vertex_data->pos_y[face->idx_a[i]];
            object->triangles[i].a.z = vertex_data->pos_z[face->idx_a[i]];
            object->triangles[i].b.x = vertex_data->pos_x[face->idx_b[i]];
            object->triangles[i].b.y = vertex_data->pos_y[face->idx_b[i]];
            object->triangles[i].b.z = vertex_data->pos_z[face->idx_b[i]];
            object->triangles[i].c.x = vertex_data->pos_x[face->idx_c[i]];
            object->triangles[i].c.y = vertex_data->pos_y[face->idx_c[i]];
            object->triangles[i].c.z = vertex_data->pos_z[face->idx_c[i]];
            object->properties[i].normal_a.x = vertex_data->norm_x[face->idx_a[i]];
            object->properties[i].normal_a.y = vertex_data->norm_y[face->idx_a[i]];
            object->properties[i].normal_a.z = vertex_data->norm_z[face->idx_a[i]];
            object->properties[i].normal_b.x = vertex_data->norm_x[face->idx_b[i]];
            object->properties[i].normal_b.y = vertex_data->norm_y[face->idx_b[i]];
            object->properties[i].normal_b.z = vertex_data->norm_z[face->idx_b[i]];
            object->properties[i].normal_c.x = vertex_data->norm_x[face->idx_c[i]];
            object->properties[i].normal_c.y = vertex_data->norm_y[face->idx_c[i]];
            object->properties[i].normal_c.z = vertex_data->norm_z[face->idx_c[i]];
            object->properties[i].texcoord_a.x = vertex_data->tex_u[face->idx_a[i]];
            object->properties[i].texcoord_a.y = vertex_data->tex_v[face->idx_a[i]];
            object->properties[i].texcoord_b.x = vertex_data->tex_u[face->idx_b[i]];
            object->properties[i].texcoord_b.y = vertex_data->tex_v[face->idx_b[i]];
            object->properties[i].texcoord_c.x = vertex_data->tex_u[face->idx_c[i]];
            object->properties[i].texcoord_c.y = vertex_data->tex_v[face->idx_c[i]];
        }

        //
        // Reading materials
        //
        const int material_idx = _apollo_model->meshes[m].material_id;
        const ApolloMaterial& material = _apollo_materials[material_idx];
        object->material.ior = 1.5;
        // TODO
        object->material.enable_bump_map_attr = 0;
        object->material.enable_normal_map_attr = 0;
        READ_ATTR ( object->material.emissive, material.emissive, _apollo_textures );

        switch ( material.bsdf ) {
            case APOLLO_SPECULAR: {
                // Log::info ( FMT ( "Loading specular material" ) );
                TerraAttribute albedo, specular_color, specular_intensity;
                READ_ATTR ( albedo, material.diffuse, _apollo_textures );
                READ_ATTR ( specular_color, material.specular, _apollo_textures );
                READ_ATTR ( specular_intensity, material.specular_exp, _apollo_textures );
                object->material.attributes[TERRA_PHONG_ALBEDO] = albedo;
                object->material.attributes[TERRA_PHONG_SPECULAR_COLOR] = specular_color;
                object->material.attributes[TERRA_PHONG_SPECULAR_INTENSITY] = specular_intensity;
                object->material.attributes_count = TERRA_PHONG_END;
                terra_bsdf_phong_init ( &object->material.bsdf );
                break;
            }

            default:
            case APOLLO_MIRROR:
                Log::warning ( FMT ( "Scene(%s) Unsupported mirror material(%s). Defaulting to diffuse." ), _apollo_model->name, material.name );

            case APOLLO_PBR:
                Log::warning ( FMT ( "Scene(%s) Unsupported pbr material(%s). Defaulting to diffuse" ), _apollo_model->name, material.name );

            case APOLLO_DIFFUSE: {
                // Log::info ( FMT ( "Loading diffuse material" ) );
                TerraAttribute albedo;
                READ_ATTR ( albedo, material.diffuse, _apollo_textures );
                object->material.attributes[TERRA_DIFFUSE_ALBEDO] = albedo;
                object->material.attributes_count = TERRA_DIFFUSE_END;
                terra_bsdf_diffuse_init ( &object->material.bsdf );
                break;
            }
        }

        n_triangles += object->triangles_count;
    }

    // TODO free materials/textures?
    Log::info ( FMT ( "Building acceleration structure for %zu triangles", n_triangles ) );
    terra_scene_commit ( _scene );
    // Log::info(FMT("Finished importing %s. objects(%d) textures(%d)", _apollo_model->name, terra_scene_count_objects(_scene), _textures.size()));
    Log::info ( FMT ( "Finished building %s", _apollo_model->name ) );
    //
    _vert_gens.clear();
    _vert_gens.resize ( _apollo_model->vertex_count, 0 );
    _gen = 0;
    return true;
}

bool Scene::load ( const char* filename ) {
    if ( !_load_scene ( filename ) ) {
        return false;
    }

    if ( !_build_scene() ) {
        return false;
    }

    return true;
}

bool Scene::move_mesh ( const char* name, const TerraFloat3* pos ) {
    ++_gen;

    for ( size_t i = 0; i < _apollo_model->mesh_count; ++i ) {
        ApolloMesh* m = &_apollo_model->meshes[i];

        if ( strcmp ( name, m->name ) == 0 ) {
            TerraFloat3 current_pos = terra_f3_set ( m->bounding_sphere[0], m->bounding_sphere[1], m->bounding_sphere[2] );
            TerraFloat3 delta = terra_subf3 ( pos, &current_pos );
            const ApolloModelVertexData* data = &_apollo_model->vertex_data;
            const ApolloMeshFaceData* face = &m->face_data;

            for ( size_t j = 0; j < m->face_count; ++j ) {
                if ( _vert_gens[face->idx_a[j]] < _gen ) {
                    data->pos_x[face->idx_a[j]] += delta.x;
                    data->pos_y[face->idx_a[j]] += delta.y;
                    data->pos_z[face->idx_a[j]] += delta.z;
                    _vert_gens[face->idx_a[j]] = _gen;
                }

                if ( _vert_gens[face->idx_b[j]] < _gen ) {
                    data->pos_x[face->idx_b[j]] += delta.x;
                    data->pos_y[face->idx_b[j]] += delta.y;
                    data->pos_z[face->idx_b[j]] += delta.z;
                    _vert_gens[face->idx_b[j]] = _gen;
                }

                if ( _vert_gens[face->idx_c[j]] < _gen ) {
                    data->pos_x[face->idx_c[j]] += delta.x;
                    data->pos_y[face->idx_c[j]] += delta.y;
                    data->pos_z[face->idx_c[j]] += delta.z;
                    _vert_gens[face->idx_c[j]] = _gen;
                }
            }

            m->bounding_sphere[0] += delta.x;
            m->bounding_sphere[1] += delta.y;
            m->bounding_sphere[2] += delta.z;
            m->aabb_min[0] += delta.x;
            m->aabb_min[1] += delta.y;
            m->aabb_min[2] += delta.z;
            m->aabb_max[0] += delta.x;
            m->aabb_max[1] += delta.y;
            m->aabb_max[2] += delta.z;
            _build_scene();
            return true;
        }
    }

    return false;
}

size_t Scene::get_mesh_states ( ObjectState* states, size_t cap ) {
    size_t i = 0;

    for ( ; i < _apollo_model->mesh_count && i < cap; ++i ) {
        states[i].name = _apollo_model->meshes[i].name;
        states[i].x = _apollo_model->meshes[i].bounding_sphere[0];
        states[i].y = _apollo_model->meshes[i].bounding_sphere[1];
        states[i].z = _apollo_model->meshes[i].bounding_sphere[2];
    }

    return i;
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
    Log::info ( FMT ( "Samples per pixel = %d (", _opts.samples_per_pixel ) );
    Log::info ( FMT ( "Accelerator       = %s", Scene::from_terra_accelerator ( _opts.accelerator ) ) );
    Log::info ( FMT ( "Sampling          = %s", Scene::from_terra_sampling ( _opts.sampling_method ) ) );
    Log::info ( FMT ( "Bounces           = %d", _opts.bounces ) );
    Log::info ( FMT ( "Subpixel jitter   = %f", _opts.subpixel_jitter ) );
    Log::info ( FMT ( "Screen gamma      = %f", _opts.gamma ) );
    Log::info ( FMT ( "Exposure          = %f", _opts.manual_exposure ) );
    Log::info ( FMT ( "Tonemap           = %s", Scene::from_terra_tonemap ( _opts.tonemapping_operator ) ) );
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