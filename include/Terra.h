#ifndef _TERRA_H_
#define _TERRA_H_

#ifdef __cplusplus
extern "C" {
#endif

// std
#include <stdlib.h>

// Terra
#include "TerraMath.h"

/*
    Terra rendering options
    This is a list of available options that can be set in `TerraSceneOptions` used when calling `terra_trace()`

*/
#define terra_bsdf_importance_sample 0

//--------------------------------------------------------------------------------------------------
// Shading Types
//--------------------------------------------------------------------------------------------------
#ifndef TERRA_MATERIAL_MAX_ATTRIBUTES
#define TERRA_MATERIAL_MAX_ATTRIBUTES 8
#endif

#ifndef TERRA_MATERIAL_MAX_LAYERS
#define TERRA_MATERIAL_MAX_LAYERS 4
#endif

typedef struct {
    TerraFloat4x4 transform;
    TerraFloat3   normal;
    TerraFloat3   emissive;
    float         ior;
    TerraFloat3   attributes[TERRA_MATERIAL_MAX_ATTRIBUTES];
} TerraShadingSurface;

typedef TerraFloat3 ( TerraBSDFSampleRoutine ) ( const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo );
typedef float       ( TerraBSDFPdfRoutine )    ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo );
typedef TerraFloat3 ( TerraBSDFEvalRoutine )   ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo );

typedef struct {
    TerraBSDFSampleRoutine* sample;
    TerraBSDFPdfRoutine*    pdf;
    TerraBSDFEvalRoutine*   eval;
} TerraBSDF;

// Sampling filter to be applied. Trilinear & Anisotropic enable ray differentials and mipmap generation at scene_end()
typedef enum {
    kTerraFilterPoint,
    kTerraFilterBilinear,
    kTerraFilterTrilinear,
    kTerraFilterAnisotropic
} TerraFilter;

// How to handle out of bound texture coordinates
typedef enum {
    kTerraTextureAddressWrap,
    kTerraTextureAddressMirror,
    kTerraTextureAddressClamp
} TerraTextureAddressMode;

// A texture is invalid if pixels is NULL
typedef struct {
    void*    pixels;
    uint16_t width;
    uint16_t height;
    uint8_t  components;
    uint8_t  depth;
    uint8_t  filter;        // kTerraTextureFilter
    uint8_t  address_mode;  // kTerraTextureAddressMode
} TerraTexture;

typedef void        ( *TerraAttributeFinalize ) ( void* attribute );
typedef TerraFloat3 ( *TerraAttributeEval )     ( void* attribute, const void* texcoord, const void* world_pos );
typedef struct {
    void*                    state;
    TerraAttributeFinalize   finalize;
    TerraAttributeEval       eval;
    TerraFloat3              value;
} TerraAttribute;

typedef struct {
    TerraBSDF      bsdf;
    float          ior;
    TerraAttribute emissive;
    TerraAttribute attributes[TERRA_MATERIAL_MAX_ATTRIBUTES];
    size_t         attributes_count;
    bool           enable_bump_map_attr;
    bool           enable_normal_map_attr;
} TerraMaterial;

//--------------------------------------------------------------------------------------------------
// Geometric types ( Scene )
//--------------------------------------------------------------------------------------------------
typedef struct TerraAABB {
    TerraFloat3 min;
    TerraFloat3 max;
} TerraAABB;

typedef struct {
    TerraFloat3 a;
    TerraFloat3 b;
    TerraFloat3 c;
} TerraTriangle;

typedef struct {
    TerraFloat3 normal_a;
    TerraFloat3 normal_b;
    TerraFloat3 normal_c;
    TerraFloat2 texcoord_a;
    TerraFloat2 texcoord_b;
    TerraFloat2 texcoord_c;
} TerraTriangleProperties;

typedef struct {
    TerraTriangle*           triangles;
    TerraTriangleProperties* properties;
    size_t                   triangles_count;
    TerraMaterial            material;
} TerraObject;

typedef enum {
    kTerraTonemappingOperatorNone,
    kTerraTonemappingOperatorLinear,
    kTerraTonemappingOperatorReinhard,
    kTerraTonemappingOperatorFilmic,
    kTerraTonemappingOperatorUncharted2
} TerraTonemappingOperator;

typedef enum {
    kTerraAcceleratorBVH
} TerraAccelerator;

typedef enum {
    kTerraSamplingMethodRandom,
    kTerraSamplingMethodStratified,
    kTerraSamplingMethodHalton
} TerraSamplingMethod;

typedef enum {
    kTerraIntegratorSimple,
    kTerraIntegratorDirect,
    kTerraIntegratorDirectMis,
    kTerraIntegratorDebugMono,
    kTerraIntegratorDebugDepth,
    kTerraIntegratorDebugNormals,
} TerraIntegrator;

typedef struct {
    TerraAttribute              environment_map;
    TerraTonemappingOperator    tonemapping_operator;
    TerraAccelerator            accelerator;
    TerraSamplingMethod         sampling_method;
    TerraIntegrator             integrator;

    float   subpixel_jitter;
    size_t  samples_per_pixel;
    size_t  bounces;
    size_t  strata;

    float   manual_exposure;
    float   gamma;
} TerraSceneOptions;

// Scene
typedef struct {
    TerraFloat3 position;
    TerraFloat3 direction;
    TerraFloat3 up;
    float       fov;
} TerraCamera;

typedef struct {
    TerraFloat3 acc;
    int         samples;
} TerraRawIntegrationResult;

typedef struct {
    TerraFloat3*               pixels;
    TerraRawIntegrationResult* results;
    size_t                     width;
    size_t                     height;
} TerraFramebuffer;

typedef struct {
    uint32_t object_idx : 8;
    uint32_t triangle_idx : 24;
} TerraPrimitiveRef;

//--------------------------------------------------------------------------------------------------
// Terra public API
//--------------------------------------------------------------------------------------------------
typedef void*   HTerraScene;

HTerraScene         terra_scene_create();
TerraObject*        terra_scene_add_object ( HTerraScene scene, size_t triangle_count );
size_t              terra_scene_count_objects ( HTerraScene scene );
void                terra_scene_commit ( HTerraScene scene );
void                terra_scene_clear ( HTerraScene scene );
TerraSceneOptions*  terra_scene_get_options ( HTerraScene scene );
void                terra_scene_destroy ( HTerraScene scene );

bool                terra_framebuffer_create ( TerraFramebuffer* framebuffer, size_t width, size_t height );
void                terra_framebuffer_clear ( TerraFramebuffer* framebuffer );
void                terra_framebuffer_destroy ( TerraFramebuffer* framebuffer );

bool                terra_texture_init ( TerraTexture* texture, size_t width, size_t height, size_t components, const void* data );
bool                terra_texture_init_hdr ( TerraTexture* texture, size_t width, size_t height, size_t components, const float* data );
TerraFloat3         terra_texture_read ( TerraTexture* texture, size_t x, size_t y );
TerraFloat3         terra_texture_sample ( void* texture, const void* uv, const void* xyz );
TerraFloat3         terra_texture_sample_latlong ( void* texture, const void* dir, const void* xyz );
void                terra_texture_destroy ( TerraTexture* texture );
void                terra_texture_finalize ( void* texture );

void                terra_attribute_init_constant ( TerraAttribute* attr, const TerraFloat3* value );
void                terra_attribute_init_texture ( TerraAttribute* attr, TerraTexture* texture );
void                terra_attribute_init_cubemap ( TerraAttribute* attr, TerraTexture* texture );

void                terra_render ( const TerraCamera* camera, HTerraScene scene, const TerraFramebuffer* framebuffer, size_t x, size_t y, size_t width, size_t height );

//--------------------------------------------------------------------------------------------------
// Terra system API
//--------------------------------------------------------------------------------------------------
// Client can override by defining TERRA_MALLOC
//------------------------------------------------------------------------------------------------
void*               terra_malloc ( size_t size );
void*               terra_realloc ( void* ptr, size_t size );
void                terra_free ( void* ptr );

//--------------------------------------------------------------------------------------------------
// Client can override by defining TERRA_LOG
//--------------------------------------------------------------------------------------------------
void                terra_log ( const char* str, ... );

#ifdef __cplusplus
}
#endif

#endif // _TERRA_H_