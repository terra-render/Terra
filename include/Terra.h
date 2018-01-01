#ifndef _TERRA_H_
#define _TERRA_H_

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
// Context
typedef struct TerraShadingContext
{
    TerraFloat2   texcoord;
    TerraFloat3   normal;
    TerraFloat3   view; // w_o
    TerraFloat4x4 rot;
}TerraShadingContext;

// Some state shared inside a BSDF.
typedef struct TerraShadingState
{
    TerraFloat3 half_vector;
    TerraFloat3 albedo;
    float       roughness;
    float       metalness;
    float       emissive;
    float       fresnel;
}TerraShadingState;

struct  TerraMaterial;
typedef TerraFloat3 (TerraRoutineSample) (const struct TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3);
typedef float       (TerraRoutineWeight) (const struct TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx);
typedef TerraFloat3 (TerraRoutineShade)  (const struct TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx);

typedef enum TerraBSDFType 
{
    kTerraBSDFDiffuse,
    kTerraBSDFRoughDielectric,
    kTerraBSDFGlass
} TerraBSDFType;

typedef struct TerraBSDF
{
    TerraRoutineSample* sample;
    TerraRoutineWeight* weight;
    TerraRoutineShade*  shade;
    TerraBSDFType type;
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
    TerraBSDF bsdf;

    TerraAttribute albedo;
    TerraAttribute roughness;
    TerraAttribute metalness;
    TerraAttribute emissive;
    float ior;
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
    TerraTriangle* triangles;
    TerraTriangleProperties* properties;
    int triangles_count;
    TerraMaterial material;
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
    TerraHDRTexture environment_map;
    TerraTonemappingOperator tonemapping_operator;
    TerraAccelerator accelerator;

    bool  enable_direct_light_sampling;
    float subpixel_jitter;
    int   samples_per_pixel;
    int   bounces;

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
    int lights_count;

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
void            terra_scene_begin(TerraScene* scene, int objects_count, int materials_count);
TerraObject*    terra_scene_add_object(TerraScene* scene);
void            terra_scene_end(TerraScene* scene);
void            terra_scene_destroy(TerraScene* scene);

bool            terra_framebuffer_create(TerraFramebuffer* framebuffer, int width, int height);
void            terra_framebuffer_destroy(TerraFramebuffer* framebuffer);

TerraStats      terra_render(const TerraCamera *camera, TerraScene *scene, const TerraFramebuffer *framebuffer, int x, int y, int width, int height);
TerraRay        terra_camera_ray(const TerraCamera* camera, const TerraFramebuffer* framebuffer, int x, int y, float jitter, const TerraFloat4x4* rot_opt);

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