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

Scene::Scene() {
    memset ( &_scene, 0, sizeof ( HTerraScene ) );
}

Scene::~Scene() {
}
#include <Windows.h>
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

    ApolloAllocator allocator;
    allocator.p = NULL;
    allocator.alloc = apollo_alloc;
    allocator.realloc = apollo_realloc;
    allocator.free = apollo_free;
    free ( _apollo_model );
    apollo_buffer_free ( _apollo_materials, &allocator );
    apollo_buffer_free ( _apollo_textures, &allocator );
    _apollo_model = ( ApolloModel* ) malloc ( sizeof ( ApolloModel ) );
    _apollo_materials = NULL;
    _apollo_textures = NULL;
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
    options.temp_allocator = allocator;
    options.final_allocator = allocator;
    options.prealloc_vertex_count = 1 << 18;
    options.prealloc_index_count = 1 << 18;
    options.prealloc_mesh_count = 16;
    Log::info ( FMT ( "Importing model file %s", filename ) );
    char dir[256];
    GetCurrentDirectory ( 256, dir );
    Log::info ( FMT ( "Working dir: %s", dir ) );

    if ( apollo_import_model_obj ( filename, _apollo_model, &_apollo_materials, &_apollo_textures, &options ) != APOLLO_SUCCESS ) {
        Log::error ( FMT ( "Failed to import %s", filename ) );
        return false;
    }

    _path = filename;
    int bsdf_count[APOLLO_BSDF_COUNT];
    memset ( bsdf_count, 0, sizeof ( int ) * APOLLO_BSDF_COUNT );

    for ( size_t i = 0; i < apollo_buffer_size ( _apollo_materials ); ++i ) {
        bsdf_count[_apollo_materials[i].bsdf] += 1;
    }

    _name = std::string ( _apollo_model->name );
    Log::info ( FMT ( "Finished importing %s.", filename ) );
    Log::info ( FMT ( "Meshes(%d) [ pbr(%d) diffuse(%d) specular(%d) mirror(%d) ], textures(%d)",
                      _apollo_model->mesh_count, bsdf_count[APOLLO_PBR], bsdf_count[APOLLO_DIFFUSE], bsdf_count[APOLLO_SPECULAR], bsdf_count[APOLLO_MIRROR], _textures.size() ) );
    // Try loading a config
    std::string config_path ( _apollo_model->dir );
    config_path += _apollo_model->name;
    config_path += ".config";

    if ( Config::load ( config_path.c_str() ) ) {
        Log::info ( FMT ( "%s config loaded.", _apollo_model->name ) );
    }

    return true;
}

bool Scene::_build_scene() {
    if ( _first_load ) {
        _first_load = false;
        _read_config();
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

bool Scene::mesh_exists ( const char* name ) {
    for ( size_t i = 0; i < _apollo_model->mesh_count; ++i ) {
        ApolloMesh* m = &_apollo_model->meshes[i];

        if ( strcmp ( name, m->name ) == 0 ) {
            return true;
        }
    }

    return false;
}

bool Scene::move_mesh ( const char* name, const TerraFloat3& pos ) {
    ++_gen;

    for ( size_t i = 0; i < _apollo_model->mesh_count; ++i ) {
        ApolloMesh* m = &_apollo_model->meshes[i];

        if ( strcmp ( name, m->name ) == 0 ) {
            TerraFloat3 current_pos = terra_f3_set ( m->bounding_sphere[0], m->bounding_sphere[1], m->bounding_sphere[2] );
            TerraFloat3 delta = terra_subf3 ( &pos, &current_pos );
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

void Scene::get_mesh_states ( std::vector<ObjectState>& states ) {
    size_t i = 0;
    states.clear ();
    states.reserve ( _apollo_model->mesh_count );

    for ( ; i < _apollo_model->mesh_count; ++i ) {
        states.emplace_back();
        ObjectState& state = states.back();
        state.name = _apollo_model->meshes[i].name;
        state.x = _apollo_model->meshes[i].bounding_sphere[0];
        state.y = _apollo_model->meshes[i].bounding_sphere[1];
        state.z = _apollo_model->meshes[i].bounding_sphere[2];
    }
}

void Scene::clear() {
    if ( _scene != NULL ) {
        terra_scene_clear ( _scene );
    }

    _textures.clear();
    memset ( &_scene, 0, sizeof ( HTerraScene ) );
}

void Scene::_read_config() {
    string tonemap_str     = Config::read_s ( Config::RENDER_TONEMAP );
    string accelerator_str = Config::read_s ( Config::RENDER_ACCELERATOR );
    string sampling_str    = Config::read_s ( Config::RENDER_SAMPLING );
    string integrator_str = Config::read_s ( Config::RENDER_INTEGRATOR );
    TerraTonemappingOperator tonemap = Config::to_terra_tonemap ( tonemap_str );
    TerraAccelerator accelerator     = Config::to_terra_accelerator ( accelerator_str );
    TerraSamplingMethod sampling     = Config::to_terra_sampling ( sampling_str );
    TerraIntegrator integrator       = Config::to_terra_integrator ( integrator_str );

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

    if ( integrator == -1 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_INTEGRATOR value %s. Defaulting to simple.", integrator_str.c_str() ) );
        integrator = kTerraIntegratorSimple;
    }

    int bounces = Config::read_i ( Config::RENDER_MAX_BOUNCES );
    int samples = Config::read_i ( Config::RENDER_SAMPLES );
    float exposure = Config::read_f ( Config::RENDER_EXPOSURE );
    float gamma = Config::read_f ( Config::RENDER_GAMMA );
    float jitter = Config::read_f ( Config::RENDER_JITTER );

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

    if ( jitter < 0 ) {
        Log::error ( FMT ( "Invalid configuration RENDER_JITTER (%f < 0) Defaulting to 0.", jitter ) );
        jitter = 0;
    }

    _opts.bounces              = bounces;
    _opts.samples_per_pixel    = samples;
    _opts.subpixel_jitter      = jitter;
    _opts.tonemapping_operator = tonemap;
    _opts.manual_exposure      = exposure;
    _opts.gamma                = gamma;
    _opts.accelerator          = accelerator;
    _opts.strata               = 4;
    _opts.sampling_method      = sampling;
    _opts.integrator           = integrator;
    _envmap_color     = Config::read_f3 ( Config::RENDER_ENVMAP_COLOR );
    terra_attribute_init_constant ( &_opts.environment_map, &_envmap_color );
    _camera.fov       = Config::read_f ( Config::RENDER_CAMERA_VFOV_DEG );
    _camera.position  = Config::read_f3 ( Config::RENDER_CAMERA_POS );
    _camera.direction = Config::read_f3 ( Config::RENDER_CAMERA_DIR );
    _camera.up        = Config::read_f3 ( Config::RENDER_CAMERA_UP );
}

void Scene::update_config() {
    if ( _opts.bounces != Config::read_i ( Config::RENDER_MAX_BOUNCES )
            || _opts.samples_per_pixel != Config::read_i ( Config::RENDER_SAMPLES )
            || _opts.gamma != Config::read_f ( Config::RENDER_GAMMA )
            || _opts.manual_exposure != Config::read_f ( Config::RENDER_EXPOSURE )
            || _opts.subpixel_jitter != Config::read_f ( Config::RENDER_JITTER )
            || _opts.tonemapping_operator != Config::to_terra_tonemap ( Config::read_s ( Config::RENDER_TONEMAP ) )
            || _opts.accelerator != Config::to_terra_accelerator ( Config::read_s ( Config::RENDER_ACCELERATOR ) )
            || _opts.sampling_method != Config::to_terra_sampling ( Config::read_s ( Config::RENDER_SAMPLING ) )
            || _opts.integrator != Config::to_terra_integrator ( Config::read_s ( Config::RENDER_INTEGRATOR ) )
            || !terra_equalf3 ( &Config::read_f3 ( Config::RENDER_ENVMAP_COLOR ), &_envmap_color )
            || !terra_equalf3 ( &Config::read_f3 ( Config::RENDER_CAMERA_POS ), &_camera.position )
            || !terra_equalf3 ( &Config::read_f3 ( Config::RENDER_CAMERA_DIR ), &_camera.direction )
            || !terra_equalf3 ( &Config::read_f3 ( Config::RENDER_CAMERA_UP ), &_camera.up )
            || _camera.fov != Config::read_f ( Config::RENDER_CAMERA_VFOV_DEG )
       ) {
        _read_config();
    }

    if ( _path.compare ( Config::read_s ( Config::RENDER_SCENE_PATH ) ) != 0 ) {
        _load_scene ( Config::read_s ( Config::RENDER_SCENE_PATH ).c_str() );
    }
}

HTerraScene Scene::construct_terra_scene() {
    TerraSceneOptions* opts = terra_scene_get_options ( _scene );
    *opts = _opts;
    terra_scene_commit ( _scene );
    return _scene;
}

const TerraCamera& Scene::get_camera() {
    return _camera;
}

const TerraSceneOptions& Scene::get_options() {
    return _opts;
}

const char* Scene::name() const {
    return _name.c_str();
}

TerraTexture* Scene::_allocate_texture ( const char* path ) {
    return nullptr;
}