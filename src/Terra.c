#include <Terra.h>

#include <string.h>
#include <assert.h>

#include "TerraPrivate.h"
#include "TerraBVH.h"
#include "TerraKDTree.h"
#include "TerraPresets.h"
#include "TerraProfile.h"

//--------------------------------------------------------------------------------------------------
// Terra internal types
//--------------------------------------------------------------------------------------------------
// Light radiance (L) is stored inside the object materials as a TerraAttribute named emissive.
// This struct contains the power (or Radiant flux, Phi) of the light (radiance integrated over
// surface area and hemisphere) and the surface area.
// If emissive is a float3, power is computed as emissive * area * PI.
// If emissive is a texture, TODO (as of now it samples the middle and uses that)
typedef struct {
    TerraFloat3  power;
    float        area;
    TerraObject* object;
    float*       triangle_area;
} TerraLight;

// A copy of the current options is stored and returned when the getter is called.
// On commit it gets diffed with the one in use before updating it and the scene state is updated appropriately.
// The dirty_objects flag is set on scene object add, cleared on commit.
typedef struct {
    TerraSceneOptions   opts;
    TerraObject*        objects;
    size_t              objects_pop;
    size_t              objects_cap;
    TerraLight*         lights;
    size_t              lights_pop;
    size_t              lights_cap;
    TerraFloat3         total_light_power;
    TerraFloat3         envmap_light_power;
    union {
        TerraKDTree     kdtree;
        TerraBVH        bvh;
    };

    HTerraSpatialAcceleration acceleration;

    TerraSceneOptions   new_opts;
    bool                dirty_objects;
    bool                dirty_lights;
} TerraScene;

#define TERRA_SCENE_PREALLOCATED_OBJECTS    64
#define TERRA_SCENE_PREALLOCATED_LIGHTS     16

//--------------------------------------------------------------------------------------------------
// Terra internal routines
//--------------------------------------------------------------------------------------------------
//TerraFloat3 terra_trace ( TerraScene* scene, const TerraRay* ray, TerraSamplerRandom* random_sampler, TerraSampler2D* hemisphere_sampler );
TerraFloat3 terra_get_pixel_dir ( const TerraFramebuffer* frame, float fov, size_t x, size_t y, float jitter, float r1, float r2 );
void        terra_triangle_init_shading ( const TerraTriangle* triangle, const TerraMaterial* material, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingSurface* surface );
TerraRay    terra_camera_ray ( const TerraCamera* camera, const TerraFramebuffer* framebuffer, size_t x, size_t y, float jitter, float r1, float r2, const TerraFloat4x4* rot_opt );
TerraFloat3 terra_eval_attribute ( const TerraAttribute* attribute, const void* dir, const TerraFloat3* point );

//--------------------------------------------------------------------------------------------------
// Terra implementation
//--------------------------------------------------------------------------------------------------
#if 0
#include <immintrin.h>
__m128 terra_sse_loadf3 ( const TerraFloat3* xyz ) {
    __m128 x = _mm_load_ss ( &xyz->x );
    __m128 y = _mm_load_ss ( &xyz->y );
    __m128 z = _mm_load_ss ( &xyz->z );
    __m128 xy = _mm_movelh_ps ( x, y );
    return _mm_shuffle_ps ( xy, z, _MM_SHUFFLE ( 2, 0, 2, 0 ) );
}

__m128 terra_sse_cross ( __m128 a, __m128 b ) {
    return _mm_sub_ps (
               _mm_mul_ps ( _mm_shuffle_ps ( a, a, _MM_SHUFFLE ( 3, 0, 2, 1 ) ), _mm_shuffle_ps ( b, b, _MM_SHUFFLE ( 3, 1, 0, 2 ) ) ),
               _mm_mul_ps ( _mm_shuffle_ps ( a, a, _MM_SHUFFLE ( 3, 1, 0, 2 ) ), _mm_shuffle_ps ( b, b, _MM_SHUFFLE ( 3, 0, 2, 1 ) ) )
           );
}

float terra_sse_dotf3 ( __m128 a, __m128 b ) {
    __m128 dp = _mm_dp_ps ( a, b, 0xFF );
    return * ( float* ) &dp;
}
#endif

//--------------------------------------------------------------------------------------------------
void terra_lookatf4x4 ( TerraFloat4x4* mat_out, const TerraFloat3* normal ) {
    TerraFloat3 normalt;
    TerraFloat3 normalbt;

    if ( fabs ( normal->x ) > fabs ( normal->y ) ) {
        normalt = terra_f3_set ( normal->z, 0.f, -normal->x );
        normalt = terra_mulf3 ( &normalt, sqrtf ( normal->x * normal->x + normal->z * normal->z ) );
    } else {
        normalt = terra_f3_set ( 0.f, -normal->z, normal->y );
        normalt = terra_mulf3 ( &normalt, sqrtf ( normal->y * normal->y + normal->z * normal->z ) );
    }

    normalbt = terra_crossf3 ( normal, &normalt );
    mat_out->rows[0] = terra_f4_set ( normalt.x, normal->x, normalbt.x, 0.f );
    mat_out->rows[1] = terra_f4_set ( normalt.y, normal->y, normalbt.y, 0.f );
    mat_out->rows[2] = terra_f4_set ( normalt.z, normal->z, normalbt.z, 0.f );
    mat_out->rows[3] = terra_f4_set ( 0.f, 0.f, 0.f, 1.f );
}

//--------------------------------------------------------------------------------------------------
void terra_attribute_init_constant ( TerraAttribute* attr, const TerraFloat3* value ) {
    attr->eval = NULL;
    attr->state = NULL;
    attr->finalize = NULL;
    attr->value = *value;
}

void terra_attribute_init_texture ( TerraAttribute* attr, TerraTexture* texture ) {
    attr->state = texture;
    attr->eval = terra_texture_sample;
    attr->finalize = terra_texture_finalize;
}

void terra_attribute_init_cubemap ( TerraAttribute* attr, TerraTexture* texture ) {
    attr->state = texture;
    attr->eval = terra_texture_sample_latlong;
    attr->finalize = terra_texture_finalize;
}

void terra_triangle_init_shading ( const TerraTriangle* triangle, const TerraMaterial* material, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingSurface* surface ) {
    // Computing UV coordinates
    TerraFloat3 e0 = terra_subf3 ( &triangle->b, &triangle->a );
    TerraFloat3 e1 = terra_subf3 ( &triangle->c, &triangle->a );
    TerraFloat3 p = terra_subf3 ( point, &triangle->a );
    float d00 = terra_dotf3 ( &e0, &e0 );
    float d11 = terra_dotf3 ( &e1, &e1 );
    float d01 = terra_dotf3 ( &e0, &e1 );
    float dp0 = terra_dotf3 ( &p, &e0 );
    float dp1 = terra_dotf3 ( &p, &e1 );
    float div = d00 * d11 - d01 * d01;
    TerraFloat2 uv;
    uv.x = ( d11 * dp0 - d01 * dp1 ) / div;
    uv.y = ( d00 * dp1 - d01 * dp0 ) / div;

    // Interpolating normal at vertices
    TerraFloat3 na = terra_mulf3 ( &properties->normal_c, uv.y );
    TerraFloat3 nb = terra_mulf3 ( &properties->normal_b, uv.x );
    TerraFloat3 nc = terra_mulf3 ( &properties->normal_a, 1 - uv.x - uv.y );
    surface->normal = terra_addf3 ( &na, &nb );
    surface->normal = terra_addf3 ( &surface->normal, &nc );
    surface->normal = terra_normf3 ( &surface->normal );

    // Interpolating texcoords at vertices
    TerraFloat2 ta = terra_mulf2 ( &properties->texcoord_c, uv.y );
    TerraFloat2 tb = terra_mulf2 ( &properties->texcoord_b, uv.x );
    TerraFloat2 tc = terra_mulf2 ( &properties->texcoord_a, 1 - uv.x - uv.y );
    TerraFloat2 texcoord = terra_addf2 ( &ta, &tb );
    texcoord = terra_addf2 ( &texcoord, &tc );
    texcoord = texcoord;

    // Calculating screen space derivatives of UV coordinates
    //TerraFloat2 dduv = terra_triangle_ss_derivatives ( point, surface );

    for ( int i = 0; i < material->attributes_count; ++i ) {
        surface->attributes[i] = terra_eval_attribute ( &material->attributes[i], &texcoord, point );
    }

    surface->emissive = terra_eval_attribute ( &material->emissive, &texcoord, point );
    surface->transform = terra_f4x4_from_y ( &surface->normal );
}

//--------------------------------------------------------------------------------------------------

HTerraScene terra_scene_create() {
    TerraScene* scene = ( TerraScene* ) terra_malloc ( sizeof ( TerraScene ) );
    memset ( scene, 0, sizeof ( TerraScene ) );
    scene->objects = ( TerraObject* )  terra_malloc ( sizeof ( TerraObject ) * TERRA_SCENE_PREALLOCATED_OBJECTS );
    scene->objects_cap = TERRA_SCENE_PREALLOCATED_OBJECTS;
    scene->lights = ( TerraLight* ) terra_malloc ( sizeof ( TerraLight ) * TERRA_SCENE_PREALLOCATED_LIGHTS );
    scene->lights_cap = TERRA_SCENE_PREALLOCATED_LIGHTS;
    return scene;
}

TerraObject* terra_scene_add_object ( HTerraScene _scene, size_t triangles_count ) {
    TerraScene* scene = ( TerraScene* ) _scene;

    if ( scene->objects_pop == scene->objects_cap ) {
        scene->objects = ( TerraObject* ) terra_realloc ( scene->objects, scene->objects_cap * 2 );
        scene->objects_cap *= 2;
    }

    memset ( &scene->objects[scene->objects_pop], 0, sizeof ( TerraObject ) );
    scene->objects[scene->objects_pop].triangles = ( TerraTriangle* ) terra_malloc ( sizeof ( TerraTriangle ) * triangles_count );
    scene->objects[scene->objects_pop].properties = ( TerraTriangleProperties* ) terra_malloc ( sizeof ( TerraTriangleProperties ) * triangles_count );
    scene->objects[scene->objects_pop].triangles_count = triangles_count;
    scene->dirty_objects = true;
    scene->dirty_lights = true;     // TODO
    return &scene->objects[scene->objects_pop++];
}

size_t terra_scene_count_objects ( HTerraScene _scene ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    return scene->objects_pop;
}

void terra_scene_commit ( HTerraScene _scene ) {
    // TODO: Transform objects' vertices into world space ?
    TerraScene* scene = ( TerraScene* ) _scene;
    // Check if it is necessary to rebuild the acceleration structure.
    bool dirty_accelerator = scene->dirty_objects;

    if ( scene->opts.accelerator != scene->new_opts.accelerator ) {
        dirty_accelerator = true;
    }

    // Destroy previous acceleration structure, if necessary.
    if ( dirty_accelerator ) {
        if ( scene->opts.accelerator == kTerraAcceleratorBVH ) {
            terra_bvh_destroy ( &scene->bvh );
        } else {
            assert ( false );
        }
    }

    // Commit the new options values, lose the old ones.
    scene->opts = scene->new_opts;

    // Rebuild the acceleration structure, if necessary.
    if ( dirty_accelerator ) {
        if ( scene->opts.accelerator == kTerraAcceleratorBVH ) {
            terra_bvh_create ( &scene->bvh, scene->objects, scene->objects_pop );
        } else {
            assert ( false );
        }
    }

    // lights
    if ( scene->dirty_lights ) {
        scene->lights_pop = 0;
        scene->total_light_power = terra_f3_zero;

        for ( size_t i = 0; i < scene->objects_pop; ++i ) {
            TerraFloat2 uv = terra_f2_set ( 0.5, 0.5 );
            TerraFloat3 emissive = terra_eval_attribute ( &scene->objects[i].material.emissive, &uv, NULL );

            if ( terra_f3_is_zero ( &emissive ) ) {
                continue;
            }

            size_t idx = scene->lights_pop;
            float area = 0;
            scene->lights[idx].triangle_area = terra_malloc ( sizeof ( float ) * scene->objects[i].triangles_count );

            for ( size_t j = 0; j < scene->objects[i].triangles_count; ++j ) {
                TerraTriangle* tri = &scene->objects[i].triangles[j];
                float a = terra_triangle_area ( tri );
                scene->lights[idx].triangle_area[j] = a;
                area += a;
            }

            TerraFloat3 power = terra_mulf3 ( &emissive, area * terra_PI );
            scene->total_light_power = terra_addf3 ( &scene->total_light_power, &power );

            if ( scene->lights_pop == scene->lights_cap ) {
                scene->lights = ( TerraLight* ) terra_realloc ( scene->lights, scene->lights_cap * 2 );
                scene->lights_cap *= 2;
            }

            scene->lights[idx].object = &scene->objects[i];
            scene->lights[idx].area = area;
            scene->lights[idx].power = power;
            ++scene->lights_pop;
        }
    }

    // Clear the scene dirty flags.
    scene->dirty_objects = false;
    scene->dirty_lights = false;
}

void terra_scene_clear ( HTerraScene _scene ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    // TODO also free memory?
    scene->objects_pop = 0;

    for ( size_t i = 0; i < scene->lights_pop; ++i ) {
        terra_free ( scene->lights[i].triangle_area );
    }

    scene->lights_pop = 0;
    scene->dirty_objects = true;
}

TerraSceneOptions* terra_scene_get_options ( HTerraScene _scene ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    // Return the user copy of the scene options.
    return &scene->new_opts;
}

void terra_scene_destroy ( HTerraScene _scene ) {
    TerraScene* scene = ( TerraScene* ) _scene;

    if ( scene == NULL ) {
        assert ( false );
    }

    // Free objects
    for ( size_t i = 0; i < scene->objects_pop; ++i ) {
        terra_free ( scene->objects[i].triangles );
        terra_free ( scene->objects[i].properties );
    }

    terra_free ( scene->objects );
    terra_free ( scene->lights );

    // Free acceleration structure
    if ( scene->opts.accelerator == kTerraAcceleratorBVH ) {
        terra_bvh_destroy ( &scene->bvh );
    } else if ( scene->opts.accelerator == kTerraAcceleratorKDTree ) {
        terra_kdtree_destroy ( &scene->kdtree );
    }

    // Free scene
    terra_free ( scene );
}

void terra_ray_state_init ( TerraRayState* ray_state, const TerraRay* ray, const float footprint ) {
    ray_state->dx_origin = terra_f3_zero;
    ray_state->dy_origin = terra_f3_zero;
    ray_state->dx_direction = terra_mulf3 ( &ray->direction, footprint );
    ray_state->dy_direction = terra_mulf3 ( &ray->direction, footprint );
}

void terra_ray_state_interact_refract ( TerraRayState* state, const TerraInteraction* interaction ) {
    state->dx_origin = terra_addf3 ( &interaction->p, &interaction->dx_p );
    state->dy_origin = terra_addf3 ( &interaction->p, &interaction->dy_p );
}

void terra_ray_state_interact_reflect ( TerraRayState* state, const TerraInteraction* interaction ) {
}

void terra_ray_state_interact_surface ( TerraRayState* state, const TerraInteraction* interaction ) {

}

//--------------------------------------------------------------------------------------------------
bool terra_framebuffer_create ( TerraFramebuffer* framebuffer, size_t width, size_t height ) {
    if ( width == 0 || height == 0 ) {
        return false;
    }

    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->pixels = ( TerraFloat3* ) terra_malloc ( sizeof ( TerraFloat3 ) * width * height );
    framebuffer->results = ( TerraRawIntegrationResult* ) terra_malloc ( sizeof ( TerraRawIntegrationResult ) * width * height );

    for ( size_t i = 0; i < width * height; ++i ) {
        framebuffer->pixels[i] = terra_f3_zero;
        framebuffer->results[i].acc = terra_f3_zero;
        framebuffer->results[i].samples = 0;
    }

    return true;
}

void terra_framebuffer_clear ( TerraFramebuffer* framebuffer ) {
    for ( size_t i = 0; i < framebuffer->height; ++i ) {
        for ( size_t j = 0; j < framebuffer->width; ++j ) {
            framebuffer->pixels[i * framebuffer->width + j] = terra_f3_zero;
            framebuffer->results[i * framebuffer->width + j].acc = terra_f3_zero;
            framebuffer->results[i * framebuffer->width + j].samples = 0;
        }
    }
}

void terra_framebuffer_destroy ( TerraFramebuffer* framebuffer ) {
    if ( framebuffer == NULL ) {
        return;
    }

    terra_free ( framebuffer->results );
    terra_free ( framebuffer->pixels );
}

//--------------------------------------------------------------------------------------------------
// TODO move jitter, r1, r2 out, ad dx,dy as params
TerraFloat3 terra_get_pixel_dir ( const TerraFramebuffer* frame, float fov, size_t x, size_t y, float jitter, float r1, float r2 ) {
    float dx = -jitter + 2 * r1 * jitter;
    float dy = -jitter + 2 * r2 * jitter;
    // [0:1], y points down
    float ndc_x = ( x + 0.5f + dx ) / frame->width;
    float ndc_y = ( y + 0.5f + dy ) / frame->height;
    // [-1:1], y points up
    float screen_x = 2 * ndc_x - 1;
    float screen_y = 1 - 2 * ndc_y;
    float aspect_ratio = ( float ) frame->width / ( float ) frame->height;
    // [-aspect_ratio * tan(fov/2):aspect_ratio * tan(fov/2)]
    float frustum_x = screen_x * aspect_ratio * ( float ) tan ( ( fov * 0.0174533f ) / 2 );
    float frustum_y = screen_y * ( float ) tan ( ( fov * 0.0174533f ) / 2 );
    TerraFloat3 dir = terra_f3_set ( frustum_x, frustum_y, 1.f );
    dir = terra_normf3 ( &dir );
    return dir;
}

//--------------------------------------------------------------------------------------------------
void terra_ray_create ( TerraRay* ray, TerraFloat3* point, TerraFloat3* direction, TerraFloat3* normal, float sign ) {
    TerraFloat3 offset = terra_mulf3 ( normal, 0.0001f * sign );
    ray->origin = terra_addf3 ( point, &offset );
    ray->direction = *direction;
    ray->inv_direction.x = 1.f / ray->direction.x;
    ray->inv_direction.y = 1.f / ray->direction.y;
    ray->inv_direction.z = 1.f / ray->direction.z;
}

TerraObject* terra_find_closest ( TerraScene* scene, const TerraRay* ray, TerraRayState* ray_state, TerraShadingSurface* surface_out, TerraFloat3* intersection_point, size_t* triangle ) {
    TerraPrimitiveRef primitive;
    TerraClockTime t = TERRA_CLOCK();
    bool miss = false;

    if ( scene->opts.accelerator == kTerraAcceleratorBVH ) {
        if ( !terra_bvh_traverse ( &scene->bvh, scene->objects, ray, intersection_point, &primitive ) ) {
            miss = true;
        }
    } else {
        return NULL;
    }

    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY, TERRA_CLOCK() - t );

    if ( miss ) {
        return NULL;
    }

    TerraObject* object = &scene->objects[primitive.object_idx];

    if ( triangle ) {
        *triangle = primitive.triangle_idx;
    }

    terra_triangle_init_shading ( &object->triangles[primitive.triangle_idx], &object->material, &object->properties[primitive.triangle_idx], intersection_point, surface_out );
    return object;
}

TerraLight* terra_scene_pick_light ( const TerraScene* scene, float e, float* pdf ) {
    // e1 is a random float between 0 and 1
    /*float alpha_acc = *e1;
    size_t light_idx = SIZE_MAX;
    // TODO is this ok?
    float total_power = scene->total_light_power.x + scene->total_light_power.y + scene->total_light_power.z;

    for ( size_t i = 0; i < scene->lights_pop; ++i ) {
        float light_power = scene->lights[i].power.x + scene->lights[i].power.y + scene->lights[i].power.z;
        float alpha = light_power / total_power;
        alpha_acc -= alpha;

        if ( alpha_acc <= 0 ) {
            // We pick this light, save its index
            light_idx = i;
            // Recompute e1
            alpha_acc += alpha;
            *e1 = alpha_acc / alpha;
            break;
        }
    }

    assert ( light_idx != SIZE_MAX );
    return &scene->lights[light_idx];*/
    // TODO
    size_t i = e * scene->lights_pop;
    *pdf = 1.f / scene->lights_pop;
    return &scene->lights[i];
}

size_t terra_light_pick_triangle ( const TerraLight* light, float e, float* pdf ) {
    size_t i = e * light->object->triangles_count;
    *pdf = 1.f / light->object->triangles_count;
    return i;
}

void terra_light_sample_triangle ( const TerraLight* light, size_t triangle_idx, float e1, float e2,
                                   TerraFloat3* pos, TerraFloat2* uv, TerraFloat3* norm, float* pdf ) {
    TerraTriangle* tri = &light->object->triangles[triangle_idx];
    float s = sqrtf ( e1 );
    float a = 1 - s;
    float b = e2 * s;
    float c = 1 - a - b;
    // Pos
    TerraFloat3 pos_a = terra_mulf3 ( &tri->a, a );
    TerraFloat3 pos_b = terra_mulf3 ( &tri->b, b );
    TerraFloat3 pos_c = terra_mulf3 ( &tri->c, c );
    *pos = terra_addf3 ( &pos_a, &pos_b );
    *pos = terra_addf3 ( pos, &pos_c );
    // UV
    TerraTriangleProperties* trip = &light->object->properties[triangle_idx];
    TerraFloat2 uv_a = terra_mulf2 ( &trip->texcoord_a, a );
    TerraFloat2 uv_b = terra_mulf2 ( &trip->texcoord_b, b );
    TerraFloat2 uv_c = terra_mulf2 ( &trip->texcoord_c, c );
    *uv = terra_addf2 ( &uv_a, &uv_b );
    *uv = terra_addf2 ( uv, &uv_c );
    // Norm
    TerraFloat3 norm_a = terra_mulf3 ( &trip->normal_a, a );
    TerraFloat3 norm_b = terra_mulf3 ( &trip->normal_b, b );
    TerraFloat3 norm_c = terra_mulf3 ( &trip->normal_c, c );
    *norm = terra_addf3 ( &norm_a, &norm_b );
    *norm = terra_addf3 ( norm, &norm_c );
    *norm = terra_normf3 ( norm );
    // PDF
    *pdf = 1.f / light->triangle_area[triangle_idx];
}

float terra_luminance ( const TerraFloat3* color ) {
    return 0.212671 * color->x + 0.715160 * color->y + 0.072169 * color->z;
}

#define _randf() (float)rand() / RAND_MAX

// This
TerraFloat3 terra_compute_direct ( const TerraScene* scene, const TerraShadingSurface* surface, const TerraMaterial* material, const TerraFloat3* p, const TerraFloat3* wo ) {
    // Pick light to sample
    TerraLight* light;
    float light_pdf;
    {
        float e = _randf() - terra_Epsilon;
        light = terra_scene_pick_light ( scene, e, &light_pdf );
    }
    // Pick triangle to sample
    size_t tri_idx;
    float tri_pdf;
    {
        float e = _randf();
        tri_idx = terra_light_pick_triangle ( light, e, &tri_pdf );
    }
    // Sample triangle
    TerraFloat3 sample_pos;
    TerraFloat2 sample_uv;
    TerraFloat3 sample_norm;
    float sample_pdf;
    {
        float e1 = _randf();
        float e2 = _randf();
        terra_light_sample_triangle ( light, tri_idx, e1, e2, &sample_pos, &sample_uv, &sample_norm, &sample_pdf );
    }
    // Raycast
    TerraFloat3 p_to_light = terra_subf3 ( &sample_pos, p );
    TerraFloat3 wi = terra_normf3 ( &p_to_light );
    TerraShadingSurface light_surface;
    {
        TerraFloat3 intersection_point;
        TerraRay ray;
        terra_ray_create ( &ray, p, &wi, &surface->normal, 1 );
        TerraObject* hit = terra_find_closest ( scene, &ray, &light_surface, &intersection_point, NULL );

        if ( hit != light->object ) {
            return terra_f3_zero;
        }
    }
    // Compute reflected radiance
    float pdf;
    TerraFloat3 f;
    {
        f = material->bsdf.eval ( surface, &wi, wo );
        TerraFloat3 light_wo = terra_negf3 ( &wi );
        pdf = terra_lenf3 ( &p_to_light ) / terra_dotf3 ( &light_wo, &sample_norm ) * light_pdf * tri_pdf * sample_pdf;
    }
    TerraFloat3 L = terra_pointf3 ( &light_surface.emissive, &f );
    L = terra_mulf3 ( &L, 1 / pdf );
    return L;
}

float terra_power_heuristic ( float f, float g ) {
    return ( f * f ) / ( f * f + g * g );
}

TerraFloat3 terra_compute_direct_mis ( const TerraScene* scene, const TerraShadingSurface* surface, const TerraMaterial* material, const TerraFloat3* p, const TerraFloat3* wo ) {
    TerraFloat3 Lo = terra_f3_zero;
    TerraLight* light;
    // Sample light
    {
        // Sample and compute pdf
        float light_pdf;
        TerraFloat3 sample_pos;
        TerraFloat3 wi;
        {
            float pick_pdf;
            {
                float e = _randf() - terra_Epsilon;
                light = terra_scene_pick_light ( scene, e, &pick_pdf );
            }
            // Pick triangle to sample
            size_t tri_idx;
            float tri_pdf;
            {
                float e = _randf();
                tri_idx = terra_light_pick_triangle ( light, e, &tri_pdf );
            }
            // Sample triangle
            TerraFloat2 sample_uv;
            TerraFloat3 sample_norm;
            float sample_pdf;
            {
                float e1 = _randf();
                float e2 = _randf();
                terra_light_sample_triangle ( light, tri_idx, e1, e2, &sample_pos, &sample_uv, &sample_norm, &sample_pdf );
            }
            light_pdf = pick_pdf * tri_pdf * sample_pdf;
            TerraFloat3 p_to_light = terra_subf3 ( &sample_pos, p );
            wi = terra_normf3 ( &p_to_light );
        }
        // Shadow ray
        TerraObject* object;
        TerraShadingSurface light_surface;
        {
            TerraFloat3 intersection_point;
            TerraRay ray;
            terra_ray_create ( &ray, p, &wi, &surface->normal, 1 );
            object = terra_find_closest ( scene, &ray, &light_surface, &intersection_point, NULL );
        }

        // Go to bsdf sampling on miss
        if ( object != light->object ) {
            goto bsdf;
        }

        // If we hit the light, compute the pdf of such event
        float bsdf_pdf = material->bsdf.pdf ( surface, &wi, wo );
        // Compute weight
        float weight = terra_power_heuristic ( light_pdf, bsdf_pdf );

        // Compute reflected radiance
        if ( light_pdf != 0 ) {
            TerraFloat3 f = material->bsdf.eval ( surface, &wi, wo );
            TerraFloat3 L = terra_pointf3 ( &light_surface.emissive, &f );
            L = terra_mulf3 ( &L, terra_dotf3 ( &wi, &surface->normal ) * weight / light_pdf );
            Lo = terra_addf3 ( &Lo, &L );
        }
    }
bsdf:
    // Sample bsdf
    {
        // Sample bsdf lobe, eval, compute sample pdf
        TerraFloat3 wi;
        TerraFloat3 f;
        float bsdf_pdf;
        {
            float e1 = _randf();
            float e2 = _randf();
            float e3 = _randf();
            wi = material->bsdf.sample ( surface, e1, e2, e3, wo );
            f = material->bsdf.eval ( surface, &wi, wo );
            bsdf_pdf = material->bsdf.pdf ( surface, &wi, wo );
        }
        TerraFloat3 light_wo = terra_negf3 ( &wi );
        // Raycast the sample
        TerraShadingSurface light_surface;
        TerraObject* object;
        TerraFloat3 intersection_point;
        size_t light_triangle;
        TerraRay ray;
        {
            terra_ray_create ( &ray, p, &wi, &surface->normal, 1 );
            object = terra_find_closest ( scene, &ray, &light_surface, &intersection_point, &light_triangle );
        }

        // Go to bsdf sampling on miss
        if ( object != light->object ) {
            goto exit;
        }

        // If we hit a light, compute the pdf of such event
        float light_pdf;
        {
            if ( object == NULL ) {
                // TODO env light pdf
                goto exit;
            } else {
                float ndotw = terra_dotf3 ( &light_surface.normal, &light_wo );

                if ( ndotw <= 0 ) {
                    goto exit;
                }

                float dist = terra_sqdistf3 ( &intersection_point, p );
                light_pdf = dist / ( ndotw * terra_triangle_area ( &object->triangles[light_triangle] ) );
            }
        }
        // Compute weight
        float weight = terra_power_heuristic ( bsdf_pdf, light_pdf );
        // Fetch light radiance
        TerraFloat3 L = terra_f3_zero;
        {
            if ( object == light->object ) {
                L = light_surface.emissive;
            } else {
                // TODO env light eval
                //L = terra_eval_attribute ( &scene->opts.environment_map, &ray.direction, &intersection_point );
            }
        }

        // Compute radiance reflected
        if ( bsdf_pdf != 0 ) {
            L = terra_pointf3 ( &L, &f );
            L = terra_mulf3 ( &L, terra_dotf3 ( &wi, &surface->normal ) * weight / bsdf_pdf );
            Lo = terra_addf3 ( &Lo, &L );
        }
    }
exit:
    return Lo;
}

TerraFloat3 terra_trace ( const TerraScene* scene, const TerraRay* primary_ray ) {
    TerraFloat3 Lo = terra_f3_zero;
    TerraFloat3 throughput = terra_f3_one;
    TerraRay ray = *primary_ray;
    TerraRayState ray_state;
    memset ( &ray_state, 0, sizeof ( TerraRayState ) );
    terra_geom_ray_triangle_intersection_init ( &ray, &ray_state );
    terra_geom_ray_box_intersection_init      ( &ray, &ray_state );

    for ( size_t bounce = 0; bounce <= scene->opts.bounces; ++bounce ) {
        TerraFloat3 wo = terra_negf3 ( &ray.direction );
        // Raycast
        TerraShadingSurface surface;
        TerraFloat3 intersection_point;
        TerraObject* object;
        object = terra_find_closest ( scene, &ray, &surface, &intersection_point, NULL );

        if ( object == NULL ) {
            TerraFloat3 env_color = terra_eval_attribute ( &scene->opts.environment_map, &ray.direction, &intersection_point );
            throughput = terra_pointf3 ( &throughput, &env_color );
            //Lo = terra_addf3 ( &Lo, &throughput );
            break;
        }

#if terra_direct_lighting

        if ( bounce == 0 ) {
            Lo = terra_addf3 ( &Lo, &surface.emissive );
        }

        // Direct lighting
        {
            TerraFloat3 Ld = terra_compute_direct_mis ( scene, &surface, &object->material, &intersection_point, &wo );
            Ld = terra_pointf3 ( &Ld, &throughput );
            Lo = terra_addf3 ( &Lo, &Ld );
        }
#else // terra_direct_lighting
        TerraFloat3 emissive = surface.emissive;
        emissive = terra_pointf3 ( &throughput, &emissive );
        Lo = terra_addf3 ( &Lo, &emissive );
#endif

        // Sample BSDF for path continuation
        TerraFloat3 wi;
        float bsdf_pdf;
        {
            float e0 = _randf();
            float e1 = _randf();
            float e2 = _randf();
            wi = object->material.bsdf.sample ( &surface, e0, e1, e2, &wo );
            bsdf_pdf = terra_maxf ( object->material.bsdf.pdf ( &surface, &wi, &wo ), terra_Epsilon );
        }

        // Update throughput
        // Lo = Le + Li * f_brdf() * NdotL
        // f_brdf() = eval() / pdf()
        double NdotL = terra_maxf ( 0.f, terra_dotf3 ( &surface.normal, &wi ) );
        TerraFloat3 bsdf_eval = object->material.bsdf.eval ( &surface, &wi, &wo ); // f_brdf()
        TerraFloat3 reflectance = terra_mulf3 ( &bsdf_eval, NdotL / bsdf_pdf );    // f_brdf() * NdotL / pdf()
        throughput = terra_pointf3 ( &throughput, &reflectance );                  // Lo

        // Russian roulette for path termination
        {
            float p = terra_maxf ( throughput.x, terra_maxf ( throughput.y, throughput.z ) );
            float e3 = 0.5f;
            e3 = ( float ) rand() / RAND_MAX;

            if ( e3 > p ) {
                break;
            }

            throughput = terra_mulf3 ( &throughput, 1.f / ( p + terra_Epsilon ) );
        }
        // Next ray
        float sNoL = terra_dotf3 ( &surface.normal, &wi );
        terra_ray_create ( &ray, &intersection_point, &wi, &surface.normal, sNoL < 0.f ? -1.f : 1.f );
    }

    return Lo;
}

TerraFloat3 terra_tonemapping_uncharted2 ( const TerraFloat3* x ) {
    // http://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting
    const float A = 0.15f;
    const float B = 0.5f;
    const float C = 0.1f;
    const float D = 0.2f;
    const float E = 0.02f;
    const float F = 0.3f;
    TerraFloat3 ret;
    ret.x = ( ( x->x * ( A * x->x + C * B ) + D * E ) / ( x->x * ( A * x->x + B ) + D * F ) ) - E / F;
    ret.y = ( ( x->y * ( A * x->y + C * B ) + D * E ) / ( x->y * ( A * x->y + B ) + D * F ) ) - E / F;
    ret.z = ( ( x->z * ( A * x->z + C * B ) + D * E ) / ( x->z * ( A * x->z + B ) + D * F ) ) - E / F;
    return ret;
}

TerraFloat4x4 terra_camera_transform ( const TerraCamera* camera ) {
    TerraFloat3 zaxis = terra_normf3 ( &camera->direction );
    TerraFloat3 xaxis = terra_crossf3 ( &camera->up, &zaxis );
    xaxis = terra_normf3 ( &xaxis );
    TerraFloat3 yaxis = terra_crossf3 ( &zaxis, &xaxis );
    TerraFloat4x4 xform;
    xform.rows[0] = terra_f4_set ( xaxis.x, yaxis.x, zaxis.x, 0.f );
    xform.rows[1] = terra_f4_set ( xaxis.y, yaxis.y, zaxis.y, 0.f );
    xform.rows[2] = terra_f4_set ( xaxis.z, yaxis.z, zaxis.z, 0.f );
    xform.rows[3] = terra_f4_set ( 0.f, 0.f, 0.f, 1.f );
    return xform;
}

//--------------------------------------------------------------------------------------------------
void terra_render ( const TerraCamera* camera, HTerraScene _scene, const TerraFramebuffer* framebuffer, size_t x, size_t y, size_t width, size_t height ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    TerraClockTime t = TERRA_CLOCK();
    TerraFloat4x4 camera_xform = terra_camera_transform ( camera );
    size_t spp = scene->opts.samples_per_pixel;
    size_t samples_per_strata = 0;

    if ( scene->opts.sampling_method == kTerraSamplingMethodStratified ) {
        size_t cur = scene->opts.strata * scene->opts.strata;

        while ( ++samples_per_strata && spp > cur ) {
            cur *= cur;
        }

        spp = cur;
    }

    TerraSamplerRandom random_sampler;
    terra_sampler_random_init ( &random_sampler );

    for ( size_t i = y; i < y + height; ++i ) {
        for ( size_t j = x; j < x + width; ++j ) {
            TerraFloat3 acc = terra_f3_zero;
            // Init sampler
            TerraSampler2D hemisphere_sampler;
            hemisphere_sampler.sampler = NULL;
            TerraSamplerStratified stratified_sampler;
            TerraSamplerHalton halton_sampler;

            if ( scene->opts.sampling_method == kTerraSamplingMethodStratified ) {
                hemisphere_sampler.sampler = &stratified_sampler;
                hemisphere_sampler.sample = terra_sampler_stratified_next_pair;
                terra_sampler_stratified_init ( &stratified_sampler, &random_sampler, scene->opts.strata, 16 );
            } else if ( scene->opts.sampling_method == kTerraSamplingMethodHalton ) {
                hemisphere_sampler.sampler = &halton_sampler;
                hemisphere_sampler.sample = terra_sampler_halton_next_pair;
                terra_sampler_halton_init ( &halton_sampler );
            }

            TerraSampler2D* hemisphere_sampler_ptr = hemisphere_sampler.sampler == NULL ? NULL : &hemisphere_sampler;

            for ( size_t s = 0; s < spp; ++s ) {
                float r1 = terra_sampler_random_next ( &random_sampler );
                float r2 = terra_sampler_random_next ( &random_sampler );
                TerraRay ray = terra_camera_ray ( camera, framebuffer, j, i, scene->opts.subpixel_jitter, r1, r2, &camera_xform );
                TerraClockTime t = TERRA_CLOCK();
                TerraFloat3 cur = terra_trace ( scene, &ray );
                TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE, TERRA_CLOCK() - t );
                acc = terra_addf3 ( &acc, &cur );
            }

            // Adding any previous partial result
            TerraRawIntegrationResult* partial = &framebuffer->results[i * framebuffer->width + j];
            partial->acc = terra_addf3 ( &acc, &partial->acc );
            partial->samples += spp;
            // Calculating final radiance
            TerraFloat3 color = terra_divf3 ( &partial->acc, ( float ) partial->samples );
            // Manual exposure
            color = terra_mulf3 ( &color, scene->opts.manual_exposure );

            switch ( scene->opts.tonemapping_operator ) {
                // TODO: Should exposure be 2^exposure as with f-stops ?
                // Gamma correction
                case kTerraTonemappingOperatorLinear: {
                    color = terra_powf3 ( &color, 1.f / scene->opts.gamma );
                    break;
                }

                // Simple version, local operator w/o white balancing
                case kTerraTonemappingOperatorReinhard: {
                    // TODO: same as inv_dir invf3
                    color.x = color.x / ( 1.f + color.x );
                    color.y = color.y / ( 1.f + color.y );
                    color.z = color.z / ( 1.f + color.z );
                    color = terra_powf3 ( &color, 1.f / scene->opts.gamma );
                    break;
                }

                // Approx
                case kTerraTonemappingOperatorFilmic: {
                    // TODO: Better code pls
                    TerraFloat3 x;
                    x.x = terra_maxf ( 0.f, color.x - 0.004f );
                    x.y = terra_maxf ( 0.f, color.y - 0.004f );
                    x.z = terra_maxf ( 0.f, color.z - 0.004f );
                    color.x = ( x.x * ( 6.2f * x.x + 0.5f ) ) / ( x.x * ( 6.2f * x.x + 1.7f ) + 0.06f );
                    color.y = ( x.y * ( 6.2f * x.y + 0.5f ) ) / ( x.y * ( 6.2f * x.y + 1.7f ) + 0.06f );
                    color.x = ( x.z * ( 6.2f * x.z + 0.5f ) ) / ( x.z * ( 6.2f * x.z + 1.7f ) + 0.06f );
                    // Gamma 2.2 included
                    break;
                }

                case kTerraTonemappingOperatorUncharted2: {
                    // TODO: Should white be tweaked ?
                    // This is the white point in linear space
                    const TerraFloat3 linear_white = terra_f3_set1 ( 11.2f );
                    TerraFloat3 white_scale = terra_tonemapping_uncharted2 ( &linear_white );
                    white_scale.x = 1.f / white_scale.x;
                    white_scale.y = 1.f / white_scale.y;
                    white_scale.z = 1.f / white_scale.z;
                    const float exposure_bias = 2.f;
                    TerraFloat3 t = terra_mulf3 ( &color, exposure_bias );
                    t = terra_tonemapping_uncharted2 ( &t );
                    color = terra_pointf3 ( &t, &white_scale );
                    color = terra_powf3 ( &color, 1.f / scene->opts.gamma );
                    break;
                }

                default:
                    break;
            }

            framebuffer->pixels[i * framebuffer->width + j] = color;
        }
    }

    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER, TERRA_CLOCK() - t );
}

TerraRay terra_camera_ray ( const TerraCamera* camera, const TerraFramebuffer* framebuffer, size_t x, size_t y, float jitter, float r1, float r2, const TerraFloat4x4* camera_xform_opt ) {
    TerraFloat3 dir = terra_get_pixel_dir ( framebuffer, camera->fov, x, y, jitter, r1, r2 );

    if ( camera_xform_opt == NULL ) {
        TerraFloat4x4 xform = terra_camera_transform ( camera );
        dir = terra_transformf3 ( &xform, &dir );
    } else {
        dir = terra_transformf3 ( camera_xform_opt, &dir );
    }

    TerraRay ray;
    ray.origin = camera->position;
    ray.direction = dir;
    ray.inv_direction.x = 1.f / ray.direction.x;
    ray.inv_direction.y = 1.f / ray.direction.y;
    ray.inv_direction.z = 1.f / ray.direction.z;
    return ray;
}

//--------------------------------------------------------------------------------------------------

bool terra_texture_init ( TerraTexture* texture, size_t width, size_t height, size_t components, const void* data ) {
    texture->pixels = terra_malloc ( width * height * components );
    memcpy ( texture->pixels, data, width * height * components );
    texture->width = ( uint16_t ) width;
    texture->height = ( uint16_t ) height;
    texture->components = ( uint8_t ) components;
    texture->depth = 1;
}

bool terra_texture_init_hdr ( TerraTexture* texture, size_t width, size_t height, size_t components, const float* data ) {
    texture->pixels = terra_malloc ( sizeof ( float ) * width * height * components );
    memcpy ( texture->pixels, data, sizeof ( float ) * width * height * components );
    texture->width = ( uint16_t ) width;
    texture->height = ( uint16_t ) height;
    texture->components = ( uint8_t ) components;
    texture->depth = 4;
}

TerraFloat3 terra_texture_read ( TerraTexture* texture, size_t x, size_t y ) {
    switch ( texture->address_mode ) {
        case kTerraTextureAddressClamp:
            x = terra_mini ( x, texture->width - 1 );
            y = terra_mini ( y, texture->height - 1 );
            break;

        case kTerraTextureAddressWrap:
            x = x % texture->width;
            y = y % texture->height;
            break;

        case kTerraTextureAddressMirror: {
            if ( ( x / texture->width ) % 2 == 0 ) {
                x = x % texture->width;
                y = y % texture->height;
            } else {
                x = texture->width - ( x % texture->width );
                y = texture->height - ( y % texture->height );
            }

            break;
        }

        default:
            assert ( false );
    }

    if ( texture->depth == 1 ) {
        assert ( texture->components <= 3 );
        uint8_t* pixel = ( ( uint8_t* ) texture->pixels ) + ( y * texture->width + x ) * texture->components;
        return terra_f3_set ( pixel[0] / 255.f, pixel[1] / 255.f, pixel[2] / 255.f );
    } else if ( texture->depth == 4 ) {
        assert ( texture->components <= 3 );
        float* pixel = ( ( float* ) texture->pixels ) + ( y * texture->width + x ) * texture->components;
        return * ( TerraFloat3* ) pixel;
    }

    assert ( false );
    return terra_f3_zero;
}

TerraFloat3 terra_texture_sample ( void* _texture, const void* _uv, const void* _xyz ) {
    TerraTexture* texture = ( TerraTexture* ) _texture;
    TerraFloat2* uv = ( TerraFloat2* ) _uv;
    size_t ix = ( size_t ) uv->x;
    size_t iy = ( size_t ) uv->y;
    TerraFloat3 sample;

    switch ( texture->filter ) {
        case kTerraFilterPoint:
            sample = terra_texture_read ( texture, ix, iy );
            break;

        case kTerraFilterBilinear: {
            // TL
            size_t x1 = ix;
            size_t y1 = iy;
            // TR
            size_t x2 = terra_mini ( ix + 1, texture->width - 1 );
            size_t y2 = iy;
            // BL
            size_t x3 = ix;
            size_t y3 = terra_mini ( iy + 1, texture->height - 1 );
            // BR
            size_t x4 = terra_mini ( ix + 1, texture->width - 1 );
            size_t y4 = terra_mini ( iy + 1, texture->height - 1 );
            // Read
            TerraFloat3 n1 = terra_texture_read ( texture, x1, y1 );
            TerraFloat3 n2 = terra_texture_read ( texture, x2, y2 );
            TerraFloat3 n3 = terra_texture_read ( texture, x3, y3 );
            TerraFloat3 n4 = terra_texture_read ( texture, x4, y4 );
            // Compute weights for Bilinear filter
            float w_u = uv->x - ix;
            float w_v = uv->y - iy;
            float w_ou = 1.f - w_u;
            float w_ov = 1.f - w_v;
            // Mix
            sample.x = ( n1.x * w_ou + n2.x * w_u ) * w_ov + ( n3.x * w_ou + n4.x * w_u ) * w_v;
            sample.y = ( n1.y * w_ou + n2.y * w_u ) * w_ov + ( n3.y * w_ou + n4.y * w_u ) * w_v;
            sample.z = ( n1.z * w_ou + n2.z * w_u ) * w_ov + ( n3.z * w_ou + n4.z * w_u ) * w_v;
            break;
        }

        case kTerraFilterTrilinear:
            // TODO
            break;

        case kTerraFilterAnisotropic:
            // TODO
            break;

        default:
            assert ( false );
            break;
    }

    return sample;
}

TerraFloat3 terra_texture_sample_latlong ( void* _texture, const void* _dir, const void* _xyz ) {
    TerraTexture* texture = ( TerraTexture* ) _texture;
    TerraFloat3* dir = ( TerraFloat3* ) _dir;
    TerraFloat3 d = terra_normf3 ( dir );
    float theta = acosf ( d.y );
    float phi = atan2f ( d.z, d.x ) + terra_PI;

    TerraFloat2 uv;
    size_t u = ( phi / ( 2 * terra_PI ) ) * texture->width;
    size_t v = ( theta / ( terra_PI ) ) * texture->height;
    return terra_texture_read ( texture, u, v );
}

void terra_texture_destroy ( TerraTexture* texture ) {
    terra_free ( texture->pixels );
    texture->pixels = NULL;
}

void terra_texture_finalize ( void* _texture ) {
#ifndef TERRA_TEXTURE_NO_SRGB
    TerraTexture* texture = ( TerraTexture* ) _texture;

    if ( texture != NULL && texture->pixels != NULL ) {
        size_t size = texture->width * texture->height * texture->components;

        if ( texture->depth == 1 ) {
            for ( size_t i = 0; i < size; ++i ) {
                uint8_t* pixel = ( uint8_t* ) texture->pixels + i;
                *pixel = ( uint8_t ) ( powf ( *pixel / 255.f, 2.2f ) * 255 );
            }
        } else if ( texture->depth == 4 ) {
            for ( size_t i = 0; i < size; ++i ) {
                float* pixel = ( float* ) texture->pixels + i;
                *pixel = powf ( *pixel, 2.2f );
            }
        } else {
            assert ( false );
        }
    }

#endif
}

TerraFloat3 terra_eval_attribute ( const TerraAttribute* attribute, const void* uv, const TerraFloat3* xyz ) {
    if ( attribute->state != NULL ) {
        return attribute->eval ( attribute->state, uv, xyz );
    }

    return attribute->value;
}

//--------------------------------------------------------------------------------------------------

void terra_sampler_random_init ( TerraSamplerRandom* sampler ) {
    uint32_t seed = ( uint32_t ) ( ( uint64_t ) time ( NULL ) ^ ( uint64_t ) &exit );
    sampler->state = 0;
    sampler->inc = 1;
    terra_sampler_random_next ( sampler );
    sampler->state += seed;
    terra_sampler_random_next ( sampler );
}

void terra_sampler_random_destroy ( TerraSamplerRandom* sampler ) {
}

#pragma warning (disable : 4146)
float terra_sampler_random_next ( void* _sampler ) {
    TerraSamplerRandom* sampler = ( TerraSamplerRandom* ) _sampler;
    uint64_t old_state = sampler->state;
    sampler->state = old_state * 6364136223846793005ULL + sampler->inc;
    uint32_t xorshifted = ( uint32_t ) ( ( ( old_state >> 18 ) ^ old_state ) >> 27 );
    uint32_t rot = ( uint32_t ) ( old_state >> 59 );
    uint32_t rndi = ( xorshifted >> rot ) | ( xorshifted << ( ( -rot ) & 31 ) );
    uint64_t max = ( uint64_t ) 1 << 32;
    float resolution = 1.f / max;
    return rndi * resolution;
}

void terra_sampler_stratified_init ( TerraSamplerStratified* sampler, TerraSamplerRandom* random_sampler, size_t strata_per_dimension, size_t samples_per_stratum ) {
    sampler->random_sampler = random_sampler;
    sampler->strata = strata_per_dimension;
    sampler->samples = samples_per_stratum;
    sampler->next = 0;
    sampler->stratum_size = 1.f / sampler->strata;
}

void terra_sampler_stratified_destroy ( TerraSamplerStratified* sampler ) {
}

void terra_sampler_stratified_next_pair ( void* _sampler, float* e1, float* e2 ) {
    TerraSamplerStratified* sampler = ( TerraSamplerStratified* ) _sampler;
    assert ( sampler->next < sampler->strata * sampler->strata * sampler->samples );
    size_t stratum = sampler->next / sampler->samples;
    size_t x = stratum % sampler->strata;
    size_t y = stratum / sampler->strata;
    *e1 = terra_minf ( ( x + terra_sampler_random_next ( sampler->random_sampler ) ) * sampler->stratum_size, 1.f - terra_Epsilon );
    *e2 = terra_minf ( ( y + terra_sampler_random_next ( sampler->random_sampler ) ) * sampler->stratum_size, 1.f - terra_Epsilon );
    ++sampler->next;
}

void terra_sampler_halton_init ( TerraSamplerHalton* sampler ) {
    sampler->next = 0;
    sampler->bases[0] = 3;
    sampler->bases[1] = 2;
}

void terra_sampler_halton_destroy ( TerraSamplerHalton* sampler ) {
}

float terra_radical_inverse ( uint64_t base, uint64_t a ) {
    float inv_base = 1.f / base;
    uint64_t seq = 0;
    float denom = 1;

    while ( a ) {
        uint64_t next = a / base;
        uint64_t digit = a - next * base;
        seq = seq * base + digit;
        denom *= inv_base;
        a = next;
    }

    return terra_minf ( seq * denom, 1.f - terra_Epsilon );
}

void terra_sampler_halton_next_pair ( void* _sampler, float* e1, float* e2 ) {
    TerraSamplerHalton* sampler = ( TerraSamplerHalton* ) _sampler;
    *e1 = terra_radical_inverse ( sampler->bases[0], sampler->next );
    *e2 = terra_radical_inverse ( sampler->bases[1], sampler->next );
    ++sampler->next;
}

//--------------------------------------------------------------------------------------------------

void terra_distribution_1d_init ( TerraDistribution1D* dist, const float* f, size_t n ) {
    dist->n = n;
    dist->cdf = ( float* ) terra_malloc ( sizeof ( float ) * n );
    dist->f = ( float* ) terra_malloc ( sizeof ( float ) * n );
    float integral = 0;

    // compute the integral and the unnormalized cdf
    for ( size_t i = 0; i < n; ++i ) {
        dist->f[i] = f[i];
        integral += f[i];
        dist->cdf[i] = integral;
    }

    dist->integral = integral;

    // normalize the cdf
    for ( size_t i = 0; i < n; ++i ) {
        dist->cdf[i] /= integral;
    }
}

float terra_distribution_1d_sample ( TerraDistribution1D* dist, float e, float* pdf, size_t* idx ) {
    size_t n = dist->n;
    float prev = 0;

    for ( size_t i = 0; i < n; ++i ) {
        float curr = dist->cdf[i];

        if ( e < curr ) {
            if ( pdf != NULL ) {
                *pdf = dist->f[i] / dist->integral;
            }

            if ( idx != NULL ) {
                *idx = i;
            }

            // cdf[i - 1] <= e < cdf[i]
            // Found the bucket, now interpolate.
            float d = e - prev;
            d /= curr - prev;
            return ( i + d ) / n;
        }

        prev = curr;
    }

    assert ( false );
    return SIZE_MAX;
}

void terra_distribution_2d_init ( TerraDistributon2D* dist, const float* f, size_t nx, size_t ny ) {
    for ( size_t i = 0; i < ny; ++i ) {
        terra_distribution_1d_init ( &dist->conditionals[i], f + nx * i, nx );
    }

    // compute the marginal distribution using the integral of each row
    dist->marginal.n = ny;
    dist->marginal.f = ( float* ) terra_malloc ( sizeof ( float ) * ny );
    dist->marginal.cdf = ( float* ) terra_malloc ( sizeof ( float ) * ny );
    float integral = 0;

    for ( size_t i = 0; i < ny; ++i ) {
        dist->marginal.f[i] = dist->conditionals[i].integral;
        integral += dist->conditionals[i].integral;
        dist->marginal.cdf[i] = integral;
    }

    dist->marginal.integral = integral;

    for ( size_t i = 0; i < ny; ++i ) {
        dist->marginal.cdf[i] /= integral;
    }
}

TerraFloat2 terra_distribution_2d_sample ( TerraDistributon2D* dist, float e1, float e2, float* pdf ) {
    float pdfs[2];
    size_t i;
    float s1 = terra_distribution_1d_sample ( &dist->marginal, e1, &pdfs[0], &i );
    float s2 = terra_distribution_1d_sample ( &dist->conditionals[i], e2, &pdfs[1], NULL );

    if ( pdf != NULL ) {
        *pdf = pdfs[0] * pdfs[1];
    }

    return terra_f2_set ( s1, s2 );
}

//--------------------------------------------------------------------------------------------------

#ifndef TERRA_MALLOC
void* terra_malloc ( size_t size ) {
    return malloc ( size );
}

void* terra_realloc ( void* p, size_t size ) {
    return realloc ( p, size );
}

void terra_free ( void* ptr ) {
    free ( ptr );
}
#endif

#ifndef TERRA_LOG
#include <stdio.h>
#include <stdarg.h>
void terra_log ( const char* str, ... ) {
    va_list args;
    va_start ( args, str );
    vfprintf ( stdout, str, args );
    va_end ( args );
}
#endif

//--------------------------------------------------------------------------------------------------

float terra_triangle_area ( const TerraTriangle* tri ) {
    TerraFloat3 ab = terra_subf3 ( &tri->b, &tri->a );
    TerraFloat3 ac = terra_subf3 ( &tri->c, &tri->a );
    TerraFloat3 cross = terra_crossf3 ( &ab, &ac );
    return terra_lenf3 ( &cross ) / 2;
}

bool terra_ray_aabb_intersection ( const TerraRay* ray, const TerraAABB* aabb, float* tmin_out, float* tmax_out ) {
    float t1 = ( aabb->min.x - ray->origin.x ) * ray->inv_direction.x;
    float t2 = ( aabb->max.x - ray->origin.x ) * ray->inv_direction.x;
    float tmin = terra_minf ( t1, t2 );
    float tmax = terra_maxf ( t1, t2 );
    t1 = ( aabb->min.y - ray->origin.y ) * ray->inv_direction.y;
    t2 = ( aabb->max.y - ray->origin.y ) * ray->inv_direction.y;
    tmin = terra_maxf ( tmin, terra_minf ( t1, t2 ) );
    tmax = terra_minf ( tmax, terra_maxf ( t1, t2 ) );
    t1 = ( aabb->min.z - ray->origin.z ) * ray->inv_direction.z;
    t2 = ( aabb->max.z - ray->origin.z ) * ray->inv_direction.z;
    tmin = terra_maxf ( tmin, terra_minf ( t1, t2 ) );
    tmax = terra_minf ( tmax, terra_maxf ( t1, t2 ) );

    if ( tmax > terra_maxf ( tmin, 0.f ) ) {
        if ( tmin_out != NULL ) {
            *tmin_out = tmin;
        }

        if ( tmax_out != NULL ) {
            *tmax_out = tmax;
        }

        return true;
    }

    return false;
}

void terra_aabb_fit_triangle ( TerraAABB* aabb, const TerraTriangle* triangle ) {
    aabb->min.x = terra_minf ( aabb->min.x, triangle->a.x );
    aabb->min.x = terra_minf ( aabb->min.x, triangle->b.x );
    aabb->min.x = terra_minf ( aabb->min.x, triangle->c.x );
    aabb->min.y = terra_minf ( aabb->min.y, triangle->a.y );
    aabb->min.y = terra_minf ( aabb->min.y, triangle->b.y );
    aabb->min.y = terra_minf ( aabb->min.y, triangle->c.y );
    aabb->min.z = terra_minf ( aabb->min.z, triangle->a.z );
    aabb->min.z = terra_minf ( aabb->min.z, triangle->b.z );
    aabb->min.z = terra_minf ( aabb->min.z, triangle->c.z );
    aabb->min.x -= terra_Epsilon;
    aabb->min.y -= terra_Epsilon;
    aabb->min.z -= terra_Epsilon;
    aabb->max.x = terra_maxf ( aabb->max.x, triangle->a.x );
    aabb->max.x = terra_maxf ( aabb->max.x, triangle->b.x );
    aabb->max.x = terra_maxf ( aabb->max.x, triangle->c.x );
    aabb->max.y = terra_maxf ( aabb->max.y, triangle->a.y );
    aabb->max.y = terra_maxf ( aabb->max.y, triangle->b.y );
    aabb->max.y = terra_maxf ( aabb->max.y, triangle->c.y );
    aabb->max.z = terra_maxf ( aabb->max.z, triangle->a.z );
    aabb->max.z = terra_maxf ( aabb->max.z, triangle->b.z );
    aabb->max.z = terra_maxf ( aabb->max.z, triangle->c.z );
    aabb->max.x += terra_Epsilon;
    aabb->max.y += terra_Epsilon;
    aabb->max.z += terra_Epsilon;
}