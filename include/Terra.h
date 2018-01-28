#ifndef _TERRA_H_
#define _TERRA_H_

#ifndef TERRA_MATERIAL_MAX_ATTRIBUTES
#define TERRA_MATERIAL_MAX_ATTRIBUTES 8
#endif // TERRA_MATERIAL_MAX_ATTRIBUTES

#ifndef TERRA_MATERIAL_MAX_LAYERS
#define TERRA_MATERIAL_MAX_LAYERS 4
#endif // TERRA_MATERIAL_MAX_LAYERS

// Include
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <float.h>
#include <assert.h>
#include <stdint.h>

// Terra
#include "TerraMath.h"

//--------------------------------------------------------------------------------------------------
// Shading Types
//--------------------------------------------------------------------------------------------------
typedef struct TerraShadingSurface
{
    TerraFloat4x4 rot;
    TerraFloat3   normal;
    TerraFloat3   emissive;
    float         ior;
    TerraFloat3   attributes[TERRA_MATERIAL_MAX_ATTRIBUTES];
}TerraShadingSurface;

typedef TerraFloat3 (TerraBSDFSampleRoutine) (const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo);
typedef float       (TerraBSDFPdfRoutine)    (const TerraShadingSurface* surface, const TerraFloat3* wi,        const TerraFloat3* wo);
typedef TerraFloat3 (TerraBSDFEvalRoutine)   (const TerraShadingSurface* surface, const TerraFloat3* wi,        const TerraFloat3* wo);

typedef struct TerraBSDF
{
    TerraBSDFSampleRoutine* sample;
    TerraBSDFPdfRoutine*    pdf;
    TerraBSDFEvalRoutine*   eval;
    float                   layer_weight;
    float                   ior;
}TerraBSDF;

// All TerraTextures are LDR by default. Each component is one byte only.
// <comps> indicates how many components there are in the texture.
// <offset> indicates at what component we should start to read from.
// This are here in case a single texture is shared by multiple materials and each channel
// is used for a different component.
// A texture is invalid if pixels is NULL
typedef struct TerraTexture
{
    uint8_t* pixels;
    uint16_t width;
    uint16_t height;
    uint8_t  comps;
    uint8_t  offset;
    uint8_t  filter;
    uint8_t  address_mode;
    bool     srgb;
}TerraTexture;

// HDR textures have 32-bit float components. 'half's are not accepted and should be converted before
// filling the data in. There is no comps/offset/filtering/addressing as HDRTexture is currently
// used only as cubemap and defaults to Bilinear. The only supported format is latitude/longitude
typedef struct TerraHDRTexture
{
    float*   pixels;
    uint16_t width;
    uint16_t height;
}TerraHDRTexture;

// Sampling filter to be applied. Mipmapping is not supported, thus no trilinear/anisotropic yet
typedef enum TerraFilter
{
    kTerraFilterPoint,
    kTerraFilterBilinear,
}TerraFilter;

// How to handle out of bound texture coordinates
typedef enum TerraTextureAddressMode
{
    kTerraTextureAddressWrap,
    kTerraTextureAddressMirror,
    kTerraTextureAddressClamp
}TerraTextureAddressMode;

typedef struct TerraAttribute
{
    TerraFloat3  value;
    TerraTexture map;
}TerraAttribute;

typedef struct TerraMaterial
{
    TerraBSDF      bsdf;
    float          ior;
    TerraAttribute emissive;
    TerraAttribute attributes[TERRA_MATERIAL_MAX_ATTRIBUTES];
    int            attributes_count;
} TerraMaterial;

//--------------------------------------------------------------------------------------------------
// Geometric types ( Scene )
//--------------------------------------------------------------------------------------------------
typedef struct TerraTriangle
{
    TerraFloat3 a;
    TerraFloat3 b;
    TerraFloat3 c;
} TerraTriangle;

typedef struct TerraTriangleProperties
{
    TerraFloat3 normal_a;
    TerraFloat3 normal_b;
    TerraFloat3 normal_c;
    TerraFloat2 texcoord_a;
    TerraFloat2 texcoord_b;
    TerraFloat2 texcoord_c;
}TerraTriangleProperties;

typedef int MaterialID;

typedef struct TerraObject
{
    TerraTriangle*           triangles;
    TerraTriangleProperties* properties;
    int                      triangles_count;
    TerraMaterial            material;
}TerraObject;

typedef enum TerraTonemappingOperator
{
    kTerraTonemappingOperatorNone,
    kTerraTonemappingOperatorLinear,
    kTerraTonemappingOperatorReinhard,
    kTerraTonemappingOperatorFilmic,
    kTerraTonemappingOperatorUncharted2
}TerraTonemappingOperator;

typedef enum TerraAccelerator
{
    kTerraAcceleratorBVH,
    kTerraAcceleratorKDTree
}TerraAccelerator;

typedef struct TerraSceneOptions
{
    // lat/long format
    TerraHDRTexture          environment_map;
    TerraTonemappingOperator tonemapping_operator;
    TerraAccelerator         accelerator;

    bool  direct_sampling;
    bool  stratified_sampling;
    float subpixel_jitter;
    int   samples_per_pixel;
    int   bounces;
    int   num_strata;

    float manual_exposure;
    float gamma;
}TerraSceneOptions;

typedef struct TerraPrimitiveRef
{
    uint32_t object_idx   :  8;
    uint32_t triangle_idx : 24;
}TerraPrimitiveRef;

// scene
typedef struct TerraLight 
{
    TerraFloat3 center;
    float radius;
    TerraAABB aabb;
    float emissive;
} TerraLight;

typedef struct TerraScene
{
    TerraSceneOptions opts;

    TerraObject* objects;
    int          objects_count;

    TerraLight* lights;
    int         lights_count;

    void* _impl;
} TerraScene;

typedef struct TerraCamera
{
    TerraFloat3 position;
    TerraFloat3 direction;
    TerraFloat3 up;
    float       fov;
} TerraCamera;

typedef struct TerraRawIntegrationResult
{
    TerraFloat3 acc;
    int         samples;
}TerraRawIntegrationResult;

typedef struct TerraStats
{
    // Total time elapsed in terra_render
    double total_ms;

    // Total time spent pathtracing
    double trace_total_ms;

    // Fastest ray
    double trace_min_ms;

    // Slowest ray
    double trace_max_ms;

    // Number of rays traced
    int trace_count;
}TerraStats;

typedef struct TerraFramebuffer
{
    TerraFloat3*               pixels;
    TerraRawIntegrationResult* results;
    int width;
    int height;
} TerraFramebuffer;

//--------------------------------------------------------------------------------------------------
// Terra public API
//--------------------------------------------------------------------------------------------------
void            terra_scene_begin(TerraScene* scene, int objects_count);
TerraObject*    terra_scene_add_object(TerraScene* scene);
void            terra_scene_end(TerraScene* scene);
void            terra_scene_destroy(TerraScene* scene);

bool            terra_framebuffer_create(TerraFramebuffer* framebuffer, int width, int height);
void            terra_framebuffer_destroy(TerraFramebuffer* framebuffer);

TerraStats      terra_render(const TerraCamera *camera, TerraScene *scene, const TerraFramebuffer *framebuffer, int x, int y, int width, int height);
TerraRay        terra_camera_ray(const TerraCamera* camera, const TerraFramebuffer* framebuffer, int x, int y, float jitter, const TerraFloat4x4* rot_opt, float r1, float r2);

//--------------------------------------------------------------------------------------------------
// Terra Semi-public API (Usable from bsdf routine)
//--------------------------------------------------------------------------------------------------
TerraFloat3     terra_sample_texture(const TerraTexture* texture, const TerraFloat2* uv);
TerraFloat3     terra_sample_hdr_cubemap(const TerraHDRTexture* texture, const TerraFloat3* v);
TerraFloat3     terra_eval_attribute(const TerraAttribute* attribute, const TerraFloat2* uv);
void*           terra_malloc(size_t size);
void            terra_free(void* ptr);
bool            terra_ray_aabb_intersection(const TerraRay *ray, const TerraAABB *aabb, float* tmin_out, float* tmax_out);
bool            terra_ray_triangle_intersection(const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out, float* t_out);
void            terra_aabb_fit_triangle(TerraAABB* aabb, const TerraTriangle* triangle);

typedef int64_t TerraTimeSlice;
TerraTimeSlice  terra_timer_split();
double          terra_timer_elapsed_ms(TerraTimeSlice delta);

#endif // _TERRA_H_