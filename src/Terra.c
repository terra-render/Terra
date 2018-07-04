#include <Terra.h>

#include <string.h>

#include "TerraBVH.h"
#include "TerraKDTree.h"
#include "TerraProfile.h"
#include "TerraPresets.h"
#include "TerraPrivate.h"

//--------------------------------------------------------------------------------------------------
// Internal scene representation
// A copy of the current options is stored and returned when the getter is called.
// On commit it gets diffed with the one in use before updating it and the scene state is updated appropriately.
// The dirty_objects flag is set on scene object add, cleared on commit.
typedef struct TerraScene {
    TerraSceneOptions opts;
    TerraObject*      objects;
    size_t            objects_pop;
    size_t            objects_cap;
    TerraLight*       lights;
    size_t            lights_pop;
    size_t            lights_cap;

    union {
        TerraKDTree kdtree;
        TerraBVH    bvh;
    };

    TerraSceneOptions new_opts;
    bool              dirty_objects;

    TerraFloat3*      aa_pattern;
} TerraScene;

#define TERRA_SCENE_PREALLOCATED_OBJECTS    64
#define TERRA_SCENE_PREALLOCATED_LIGHTS     16

const TerraFloat2 TERRA_F2_ZERO = { 0.f, 0.f };
const TerraFloat3 TERRA_F3_ZERO = { 0.f, 0.f, 0.f };
const TerraFloat3 TERRA_F3_ONE  = { 1.f, 1.f, 1.f };

//--------------------------------------------------------------------------------------------------
// Rendering
//--------------------------------------------------------------------------------------------------
/*
    Main rendering routine.
    Renders the  tile specified by <x y width height>
*/
void terra_render ( const TerraCamera* camera, HTerraScene _scene, const TerraFramebuffer* framebuffer, size_t x, size_t y, size_t width, size_t height ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    TerraClockTime t = TERRA_CLOCK();
    TerraRayDifferentials ray_differentials;
    size_t spp = scene->opts.samples_per_pixel;

    TerraSamplerRandom random_sampler;
    terra_sampler_random_init ( &random_sampler, spp );

    for ( size_t i = y; i < y + height; ++i ) {
        for ( size_t j = x; j < x + width; ++j ) {
            TerraFloat3 acc = TERRA_F3_ZERO;

            for ( size_t s = 0; s < spp; ++s ) {
                TerraRay ray = terra_camera_ray ( camera, j, i, framebuffer->width, framebuffer->height, &ray_differentials );
                TerraClockTime t = TERRA_CLOCK();
                TerraFloat3 cur = terra_trace ( scene, &ray, &ray_differentials );
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

            framebuffer->pixels[i * framebuffer->width + j] = color;
        }
    }

    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER, TERRA_CLOCK() - t );
}

/*
    Applies the specified operator to the framebuffer in place.
*/
void terra_tonemap ( TerraFloat3* pixels, TerraTonemapOp op ) {

}

/*
    Casts a ray into the scene and returns the index of the first primitive hit.
*/
bool terra_pick ( TerraPrimitiveRef* ref, const TerraCamera* camera, HTerraScene _scene, size_t x, size_t y, size_t width, size_t height ) {
    TerraRay ray = terra_camera_ray ( camera, x, y, width, height, NULL );
    TerraScene* scene = ( TerraScene* ) _scene;
    TerraFloat3       unused;
    return terra_find_closest_prim ( scene, &ray, ref, &unused );
}


//--------------------------------------------------------------------------------------------------
// Scene
//--------------------------------------------------------------------------------------------------
HTerraScene terra_scene_create() {
    TerraScene* scene = ( TerraScene* ) terra_malloc ( sizeof ( TerraScene ) );
    memset ( scene, 0, sizeof ( TerraScene ) );
    scene->objects = ( TerraObject* ) terra_malloc ( sizeof ( TerraObject ) * TERRA_SCENE_PREALLOCATED_OBJECTS );
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
    return &scene->objects[scene->objects_pop++];
}

size_t terra_scene_count_objects ( HTerraScene _scene ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    return scene->objects_pop;
}

void terra_scene_commit ( HTerraScene _scene ) {
    // TODO: Transform objects' vertices into world space ?
    TerraScene* scene = ( TerraScene* ) _scene;
    // Checking if it is necessary to rebuild the acceleration structure.
    bool dirty_accelerator = scene->dirty_objects;

    if ( scene->opts.accelerator != scene->new_opts.accelerator ) {
        dirty_accelerator = true;
    }

    // Commit the new options values and rebuild if necessary.
    scene->opts = scene->new_opts;

    if ( dirty_accelerator ) {
        if ( scene->opts.accelerator == kTerraAcceleratorBVH ) {
            terra_bvh_create ( &scene->bvh, scene->objects, scene->objects_pop );
        } else if ( scene->opts.accelerator == kTerraAcceleratorKDTree ) {
            terra_kdtree_create ( &scene->kdtree, scene->objects, scene->objects_pop );
        }
    }

    // TODO lights
    //
    // Clear the scene dirty objects flag.
    scene->dirty_objects = false;

    // Precomputing aa pattern
}

void terra_scene_clear ( HTerraScene _scene ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    // TODO also free memory?
    scene->objects_pop = 0;
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

TerraObject* terra_scene_object ( HTerraScene _scene, uint32_t object_idx ) {
    TerraScene* scene = ( TerraScene* ) _scene;
    return &scene->objects[object_idx];
}

//--------------------------------------------------------------------------------------------------
// Framebuffer
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
        framebuffer->pixels[i] = TERRA_F3_ZERO;
        framebuffer->results[i].acc = TERRA_F3_ZERO;
        framebuffer->results[i].samples = 0;
    }

    return true;
}

void terra_framebuffer_clear ( TerraFramebuffer* framebuffer, const TerraFloat3* value ) {
    for ( size_t i = 0; i < framebuffer->height; ++i ) {
        for ( size_t j = 0; j < framebuffer->width; ++j ) {
            framebuffer->pixels[i * framebuffer->width + j] = *value;
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
// Textures
//--------------------------------------------------------------------------------------------------
bool terra_texture_create ( TerraTexture* texture, const uint8_t* data, size_t width, size_t height, size_t components, size_t sampler, TerraTextureAddress address ) {
    return terra_texture_create_any ( texture, data, width, height, 1, components, sampler, address );
}

bool terra_texture_create_fp ( TerraTexture* texture, const float* data, size_t width, size_t height, size_t components, size_t sampler, TerraTextureAddress address ) {
    return terra_texture_create_any ( texture, data, width, height, 4, components, sampler, address );
}

void terra_texture_destroy ( TerraTexture* texture ) {
    if ( !texture ) {
        return;
    }

    if ( texture->sampler & kTerraSamplerTrilinear ) {
        size_t n_mipmaps = 1 + terra_mips_count ( texture->width, texture->height );

        for ( size_t i = 1; i < n_mipmaps; ++i ) {
            terra_map_destroy ( texture->mipmaps + i );
        }
    }

    TERRA_ASSERT ( texture->mipmaps );
    terra_map_destroy ( texture->mipmaps );
    free ( texture->mipmaps ); // mip0 should be preset

    if ( texture->ripmaps ) {
        size_t n_ripmaps = terra_rips_count ( texture->width, texture->height );

        for ( size_t i = 0; i < n_ripmaps; ++i ) {
            terra_map_destroy ( texture->ripmaps + i );
        }

        free ( texture->ripmaps );
    }
}


//--------------------------------------------------------------------------------------------------
// Attributes
//--------------------------------------------------------------------------------------------------
/*
*  Creates a constant value attribute
*/
void terra_attribute_init_constant ( TerraAttribute* attr, const TerraFloat3* constant ) {
    TERRA_ASSERT ( attr && constant );
    attr->constant._flag = TERRA_ATTR_CONSTANT_FLAG;
    attr->constant.constant = *constant;
}

/*
* Creates a constant
*/
void terra_attribute_init_texture ( TerraAttribute* attr, TerraTexture* texture ) {
    TERRA_ASSERT ( attr && texture );
    attr->fn.state = ( intptr_t ) texture;
    attr->fn.eval = terra_texture_sampler ( texture );
}

/*
* Returns true if `attr` was initialized by `_attribute_init_constant`
*/
bool terra_attribute_is_constant ( const TerraAttribute* attr ) {
    TERRA_ASSERT ( attr );
    return attr->constant._flag == 0xffffffff;
}

//--------------------------------------------------------------------------------------------------
// Materials
//--------------------------------------------------------------------------------------------------
/*
* Initializes a material with default values
*/
const TerraFloat3 kTerraMaterialDefaultIor = { 1.5f, 1.5f, 1.5f };
void terra_material_init ( TerraMaterial* material ) {
    TERRA_ASSERT ( material );

    terra_attribute_init_constant ( &material->attrs[TerraAttributeIor], &kTerraMaterialDefaultIor );
    terra_attribute_init_constant ( &material->attrs[TerraAttributeEmissive], &TERRA_F3_ZERO );
    terra_attribute_init_constant ( &material->attrs[TerraAttributeBumpMap], &TERRA_F3_ZERO );
    terra_attribute_init_constant ( &material->attrs[TerraAttributeNormalMap], &TERRA_F3_ZERO );
    material->bsdf.attrs_count = 0;
    material->_flags = 0;
}

/*
*  Sets a bsdf attribute.
*  material.bsdf should have been initialized
*/
void terra_material_bsdf_attr ( TerraMaterial* material, TerraBSDFAttribute bsdf_attr, const TerraAttribute* attr ) {
    TERRA_ASSERT ( material && attr );
    TERRA_ASSERT ( bsdf_attr < material->bsdf.attrs_count );
    material->bsdf_attrs[bsdf_attr] = *attr;
}

void terra_material_ior ( TerraMaterial* material, const TerraAttribute* attr ) {
    TERRA_ASSERT ( material && attr );
    material->attrs[TerraAttributeIor] = *attr;
}

void terra_material_emissive ( TerraMaterial* material, const TerraAttribute* attr ) {
    TERRA_ASSERT ( material && attr );
    material->attrs[TerraAttributeEmissive] = *attr;
}

/*
*  Enables bump mapping
*/
void terra_material_bump_map ( TerraMaterial* material, const TerraAttribute* attr ) {
    TERRA_ASSERT ( material && attr );
    material->attrs[TerraAttributeBumpMap] = *attr;
}

/*
*  Enables normal mapping
*/
void terra_material_normal_map ( TerraMaterial* material, const TerraAttribute* attr ) {
    TERRA_ASSERT ( material && attr );
    material->attrs[TerraAttributeNormalMap] = *attr;
}

//--------------------------------------------------------------------------------------------------
// Internal routines implementation
//--------------------------------------------------------------------------------------------------
/*
*/
TerraFloat3 terra_attribute_eval ( const TerraAttribute* attr, const void* addr ) {
    if ( terra_attribute_is_constant ( attr ) ) {
        return attr->constant.constant;
    }

    return attr->fn.eval ( attr->fn.state, addr );
}

/*
*/
void terra_ray_create ( TerraRay* ray, TerraFloat3* point, TerraFloat3* direction, TerraFloat3* normal, float sign ) {
    TerraFloat3 offset = terra_mulf3 ( normal, 0.0001f * sign );
    ray->origin = terra_addf3 ( point, &offset );
    ray->direction = *direction;
    ray->inv_direction.x = 1.f / ray->direction.x;
    ray->inv_direction.y = 1.f / ray->direction.y;
    ray->inv_direction.z = 1.f / ray->direction.z;
}

/*

*/
TerraFloat3 terra_pixel_dir ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height ) {
    // [0:1], y points down
    float ndc_x = ( x + 0.5f ) / width;
    float ndc_y = ( y + 0.5f ) / height;
    // [-1:1], y points up
    float screen_x = 2 * ndc_x - 1;
    float screen_y = 1 - 2 * ndc_y;
    float aspect_ratio = ( float ) width / height;
    // [-aspect_ratio * tan(fov/2):aspect_ratio * tan(fov/2)]
    float camera_x = screen_x * aspect_ratio * ( float ) tan ( ( camera->fov * 0.0174533f ) / 2 );
    float camera_y = screen_y * ( float ) tan ( ( camera->fov * 0.0174533f ) / 2 );
    return terra_f3_set ( camera_x, camera_y, 1.f );
}

/*
    Initializes primary ray
*/
TerraRay terra_camera_ray ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height, TerraRayDifferentials* differentials ) {
    TerraFloat3 dir = terra_pixel_dir ( camera, x, y, width, height );

    // Constructing ray rotation matrix to view direction
    TerraFloat3 zaxis = terra_normf3 ( &camera->direction );
    TerraFloat3 xaxis = terra_crossf3 ( &camera->up, &zaxis );
    xaxis = terra_normf3 ( &xaxis );
    TerraFloat3 yaxis = terra_crossf3 ( &zaxis, &xaxis );
    TerraFloat4x4 rot;
    rot.rows[0] = terra_f4 ( xaxis.x, yaxis.x, zaxis.x, 0.f );
    rot.rows[1] = terra_f4 ( xaxis.y, yaxis.y, zaxis.y, 0.f );
    rot.rows[2] = terra_f4 ( xaxis.z, yaxis.z, zaxis.z, 0.f );
    rot.rows[3] = terra_f4 ( 0.f, 0.f, 0.f, 1.f );

    // Rotating
    dir = terra_transformf3 ( &rot, &dir );

    // Initializing ray
    TerraRay ray;
    ray.origin = camera->position;
    ray.direction = terra_normf3 ( &dir );
    ray.inv_direction.x = 1.f / ray.direction.x;
    ray.inv_direction.y = 1.f / ray.direction.y;
    ray.inv_direction.z = 1.f / ray.direction.z;

    // Initializing ray differentials
    // https://graphics.stanford.edu/papers/trd/
    //
    // position = camera->origin
    // direction = zaxis + x*xaxis + y*yaxis
    // computing dposition/[dx,dy] ddirection/d[dx,dy]
    if ( differentials ) {
        differentials->pos_dx = TERRA_F3_ZERO;
        differentials->pos_dy = TERRA_F3_ZERO;

        const float dd = terra_dotf2 ( &ray.direction, &ray.direction );
        const float sqrt_dd = sqrtf ( dd );
        const float dright = terra_dotf3 ( &ray.direction, &xaxis );
        const float dup = terra_dotf3 ( &ray.direction, &yaxis );
        const float den = sqrt_dd * sqrt_dd * sqrt_dd;

        TerraFloat3 num = terra_mulf3 ( &xaxis, dd );
        TerraFloat3 numr = terra_mulf3 ( &ray.direction, dright );
        num = terra_subf3 ( &num, &numr );
        differentials->dir_dx = terra_divf3c ( &num, &den );

        num = terra_mulf3 ( &yaxis, dd );
        numr = terra_mulf3 ( &ray.direction, dup );
        num = terra_subf3 ( &num, &numr );
        differentials->dir_dy = terra_divf3c ( &num, &den );
    }

    return ray;
}

void terra_ray_differentials_transfer ( TerraRayDifferentials* differentials ) {

}

void terra_ray_differentials_reflect ( TerraRayDifferentials* differentials ) {

}

void terra_ray_differentials_refract ( TerraRayDifferentials* differentials ) {

}

/*
*/
bool terra_ray_triangle_intersection ( const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out, float* t_out ) {
    const TerraTriangle* tri = triangle;
    TerraFloat3 e1, e2, h, s, q;
    float a, f, u, v, t;
    e1 = terra_subf3 ( &tri->b, &tri->a );
    e2 = terra_subf3 ( &tri->c, &tri->a );
    h = terra_crossf3 ( &ray->direction, &e2 );
    a = terra_dotf3 ( &e1, &h );

    if ( a > -TERRA_EPS && a < TERRA_EPS ) {
        return false;
    }

    f = 1 / a;
    s = terra_subf3 ( &ray->origin, &tri->a );
    u = f * ( terra_dotf3 ( &s, &h ) );

    if ( u < 0.0 || u > 1.0 ) {
        return false;
    }

    q = terra_crossf3 ( &s, &e1 );
    v = f * terra_dotf3 ( &ray->direction, &q );

    if ( v < 0.0 || u + v > 1.0 ) {
        return false;
    }

    t = f * terra_dotf3 ( &e2, &q );

    if ( t > 0.00001 ) {
        TerraFloat3 offset = terra_mulf3 ( &ray->direction, t );
        *point_out = terra_addf3 ( &offset, &ray->origin );

        if ( t_out != NULL ) {
            *t_out = t;
        }

        return true;
    }

    return false;
}

/*
*/
bool terra_ray_aabb_intersection ( const TerraRay* ray, const TerraAABB* aabb, float* tmin_out, float* tmax_out ) {
    float t1 = ( aabb->min.x - ray->origin.x ) * ray->inv_direction.x;
    float t2 = ( aabb->max.x - ray->origin.x ) * ray->inv_direction.x;
    float tmin = TERRA_MIN ( t1, t2 );
    float tmax = TERRA_MAX ( t1, t2 );
    t1 = ( aabb->min.y - ray->origin.y ) * ray->inv_direction.y;
    t2 = ( aabb->max.y - ray->origin.y ) * ray->inv_direction.y;
    tmin = TERRA_MAX ( tmin, TERRA_MIN ( t1, t2 ) );
    tmax = TERRA_MIN ( tmax, TERRA_MAX ( t1, t2 ) );
    t1 = ( aabb->min.z - ray->origin.z ) * ray->inv_direction.z;
    t2 = ( aabb->max.z - ray->origin.z ) * ray->inv_direction.z;
    tmin = TERRA_MAX ( tmin, TERRA_MIN ( t1, t2 ) );
    tmax = TERRA_MIN ( tmax, TERRA_MAX ( t1, t2 ) );

    if ( tmax > TERRA_MAX ( tmin, 0.f ) ) {
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

/*
*/
void terra_aabb_fit_triangle ( TerraAABB* aabb, const TerraTriangle* triangle ) {
    aabb->min.x = TERRA_MIN ( aabb->min.x, triangle->a.x );
    aabb->min.x = TERRA_MIN ( aabb->min.x, triangle->b.x );
    aabb->min.x = TERRA_MIN ( aabb->min.x, triangle->c.x );
    aabb->min.y = TERRA_MIN ( aabb->min.y, triangle->a.y );
    aabb->min.y = TERRA_MIN ( aabb->min.y, triangle->b.y );
    aabb->min.y = TERRA_MIN ( aabb->min.y, triangle->c.y );
    aabb->min.z = TERRA_MIN ( aabb->min.z, triangle->a.z );
    aabb->min.z = TERRA_MIN ( aabb->min.z, triangle->b.z );
    aabb->min.z = TERRA_MIN ( aabb->min.z, triangle->c.z );
    aabb->min.x -= TERRA_EPS;
    aabb->min.y -= TERRA_EPS;
    aabb->min.z -= TERRA_EPS;
    aabb->max.x = TERRA_MAX ( aabb->max.x, triangle->a.x );
    aabb->max.x = TERRA_MAX ( aabb->max.x, triangle->b.x );
    aabb->max.x = TERRA_MAX ( aabb->max.x, triangle->c.x );
    aabb->max.y = TERRA_MAX ( aabb->max.y, triangle->a.y );
    aabb->max.y = TERRA_MAX ( aabb->max.y, triangle->b.y );
    aabb->max.y = TERRA_MAX ( aabb->max.y, triangle->c.y );
    aabb->max.z = TERRA_MAX ( aabb->max.z, triangle->a.z );
    aabb->max.z = TERRA_MAX ( aabb->max.z, triangle->b.z );
    aabb->max.z = TERRA_MAX ( aabb->max.z, triangle->c.z );
    aabb->max.x += TERRA_EPS;
    aabb->max.y += TERRA_EPS;
    aabb->max.z += TERRA_EPS;
}

/*
*/
bool terra_find_closest ( TerraScene* scene, const TerraRay* ray, const TerraMaterial** material_out, TerraShadingSurface* surface_out, TerraFloat3* intersection_point ) {
    TerraPrimitiveRef primitive;

    if ( !terra_find_closest_prim ( scene, ray, &primitive, intersection_point ) ) {
        return false;
    }

    TerraObject* object = &scene->objects[primitive.object_idx];
    *material_out = &object->material;
    terra_triangle_init_shading ( &object->triangles[primitive.triangle_idx], &object->material, &object->properties[primitive.triangle_idx], intersection_point, surface_out );
    return true;
}

/*
*/
bool terra_find_closest_prim ( TerraScene* scene, const TerraRay* ray, TerraPrimitiveRef* ref, TerraFloat3* intersection_point ) {
    TerraClockTime t = TERRA_CLOCK();
    bool miss = false;

    if ( scene->opts.accelerator == kTerraAcceleratorBVH ) {
        if ( !terra_bvh_traverse ( &scene->bvh, scene->objects, ray, intersection_point, ref ) ) {
            miss = true;
        }
    } else if ( scene->opts.accelerator == kTerraAcceleratorKDTree ) {
        if ( !terra_kdtree_traverse ( &scene->kdtree, ray, intersection_point, ref ) ) {
            miss = true;
        }
    } else {
        return false;
    }

    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY, TERRA_CLOCK() - t );

    if ( miss ) {
        return false;
    }

    return true;
}

/*
*/
void terra_build_rotation_around_normal ( TerraShadingSurface* surface ) {
    TerraFloat3 normalt;
    TerraFloat3 normalbt;
    TerraFloat3* normal = &surface->normal;

    if ( fabs ( normal->x ) > fabs ( normal->y ) ) {
        normalt = terra_f3_set ( normal->z, 0.f, -normal->x );
        normalt = terra_mulf3 ( &normalt, sqrtf ( normal->x * normal->x + normal->z * normal->z ) );
    } else {
        normalt = terra_f3_set ( 0.f, -normal->z, normal->y );
        normalt = terra_mulf3 ( &normalt, sqrtf ( normal->y * normal->y + normal->z * normal->z ) );
    }

    normalbt = terra_crossf3 ( normal, &normalt );
    surface->rot.rows[0] = terra_f4 ( normalt.x, surface->normal.x, normalbt.x, 0.f );
    surface->rot.rows[1] = terra_f4 ( normalt.y, surface->normal.y, normalbt.y, 0.f );
    surface->rot.rows[2] = terra_f4 ( normalt.z, surface->normal.z, normalbt.z, 0.f );
    surface->rot.rows[3] = terra_f4 ( 0.f, 0.f, 0.f, 1.f );
}

/*
*/
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
    surface->uv = terra_addf2 ( &texcoord, &tc );

    // Evaluating bsdf attributes
    for ( size_t i = 0; i < material->bsdf.attrs_count; ++i ) {
        surface->bsdf_attrs[i] = terra_attribute_eval ( material->bsdf_attrs + i, &surface->uv );
    }

    // Evaluating material attributes
    for ( size_t i = 0; i < TerraAttributeCount; ++i ) {
        surface->attrs[i] = terra_attribute_eval ( material->attrs + i, surface );
    }
}

/*
*/
#define _randf() (float)rand() / RAND_MAX
TerraFloat3 terra_trace ( TerraScene* scene, const TerraRay* primary_ray, TerraFloat2* differentials ) {
    TerraFloat3 Lo = TERRA_F3_ZERO;
    TerraFloat3 throughput = TERRA_F3_ONE;
    TerraRay ray = *primary_ray;

    for ( size_t bounce = 0; bounce <= scene->opts.bounces; ++bounce ) {
        const TerraMaterial* material;
        TerraShadingSurface surface;
        TerraFloat3 intersection_point;

        if ( terra_find_closest ( scene, &ray, &material, &surface, &intersection_point ) == false ) {
            TerraFloat3 env_color = terra_attribute_eval ( &scene->opts.environment_map, &ray.direction );
            throughput = terra_pointf3 ( &throughput, &env_color );
            Lo = terra_addf3 ( &Lo, &throughput );
            break;
        }

        terra_build_rotation_around_normal ( &surface );
        TerraFloat3 wo = terra_negf3 ( &ray.direction );
        TerraFloat3 emissive = surface.attrs[TerraAttributeEmissive];
        emissive = terra_pointf3 ( &throughput, &emissive );
        Lo = terra_addf3 ( &Lo, &emissive );
        float e0 = _randf();
        float e1 = _randf();
        float e2 = _randf();
        TerraFloat3 wi = material->bsdf.sample ( &surface, e0, e1, e2, &wo );
        float       bsdf_pdf = TERRA_MAX ( material->bsdf.pdf ( &surface, &wi, &wo ), TERRA_EPS );
        float       light_pdf = 0.f;

        // BSDF Contribution
        TerraFloat3 bsdf_radiance = material->bsdf.eval ( &surface, &wi, &wo );
        float       bsdf_weight = bsdf_pdf * bsdf_pdf / ( light_pdf * light_pdf + bsdf_pdf * bsdf_pdf );
        TerraFloat3 bsdf_contribution = terra_mulf3 ( &bsdf_radiance, bsdf_weight / bsdf_pdf );
        throughput = terra_pointf3 ( &throughput, &bsdf_contribution );

        // Russian roulette
        float p = TERRA_MAX ( throughput.x, TERRA_MAX ( throughput.y, throughput.z ) );
        float e3 = 0.5f;
        e3 = ( float ) rand() / RAND_MAX;

        return bsdf_radiance;

        if ( e3 > p ) {
            break;
        }

        throughput = terra_mulf3 ( &throughput, 1.f / ( p + TERRA_EPS ) );
        // Next ray (Skip if last?)
        float sNoL = terra_dotf3 ( &surface.normal, &wi );
        terra_ray_create ( &ray, &intersection_point, &wi, &surface.normal, sNoL < 0.f ? -1.f : 1.f );
    }

    return Lo;
}

/*
*/
TerraLight* terra_light_pick_power_proportional ( const struct TerraScene* scene, float* e1 ) {
    // Compute total light emitted so that we can later weight
    // TODO cache this ?
    float total_light_power = 0;

    for ( size_t i = 0; i < scene->lights_pop; ++i ) {
        total_light_power += scene->lights[i].emissive;
    }

    // Pick a light
    // e1 is a random float between 0 and
    float alpha_acc = *e1;
    int light_idx = -1;

    for ( size_t i = 0; i < scene->lights_pop; ++i ) {
        const float alpha = scene->lights[i].emissive / total_light_power;
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

    assert ( light_idx >= 0 );
    return &scene->lights[light_idx];
}

/*
*/
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
    mat_out->rows[0] = terra_f4 ( normalt.x, normal->x, normalbt.x, 0.f );
    mat_out->rows[1] = terra_f4 ( normalt.y, normal->y, normalbt.y, 0.f );
    mat_out->rows[2] = terra_f4 ( normalt.z, normal->z, normalbt.z, 0.f );
    mat_out->rows[3] = terra_f4 ( 0.f, 0.f, 0.f, 1.f );
}

/*
*/
TerraFloat3 terra_light_sample_disk ( const TerraLight* light, const TerraFloat3* surface_point, float e1, float e2 ) {
    // pick an offset from the disk center
    TerraFloat3 disk_offset;
    disk_offset.x = light->radius * sqrtf ( e1 ) * cosf ( 2 * TERRA_PI * e2 );
    disk_offset.z = light->radius * sqrtf ( e1 ) * sinf ( 2 * TERRA_PI * e2 );
    disk_offset.y = 0;
    // direction from surface to picked light
    TerraFloat3 light_dir = terra_subf3 ( &light->center, surface_point ); // TODO flip?
    light_dir = terra_normf3 ( &light_dir );
    // move the offset from the ideal flat disk to the actual bounding sphere section disk
    TerraFloat4x4 sample_rotation;
    terra_lookatf4x4 ( &sample_rotation, &light_dir );
    disk_offset = terra_transformf3 ( &sample_rotation, &disk_offset );
    // actual sample point and direction from surface
    TerraFloat3 sample_point = terra_addf3 ( &light->center, &disk_offset );
    TerraFloat3 sample_dir = terra_subf3 ( &sample_point, surface_point );
    //float sample_dist = terra_lenf3(&sample_dir);
    sample_dir = terra_normf3 ( &sample_dir );
    return sample_point;
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