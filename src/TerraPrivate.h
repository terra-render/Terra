#ifndef _TERRA_PRIVATE_H_
#define _TERRA_PRIVATE_H_

// Terra
#include <Terra.h>

#ifdef __cplusplus
extern "C" {
#endif

// OS headers are only used by a few submodules
#ifdef TERRA_SYSTEM_HEADERS
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#error Implement
#endif
#endif

//
// Preprocessor
//
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define TERRA_ASSERT(cond) { if (!(cond)) { __debugbreak();} }
#else
#define TERRA_ASSERT(cond) { if (!(cond)) { __builtin_trap();} }
#endif

#define TERRA_ZEROMEM(p) memset(p, 0, sizeof((p)[0]))
#define TERRA_ZEROMEM_ARRAY(p, c) memset(p, 0, c * sizeof((p)[0]))

//
// Shading
//
typedef struct TerraShadingSurface { // Forward declarable
    TerraFloat4x4 rot;
    TerraFloat3   normal;
    TerraFloat2   uv;
    TerraFloat3   bsdf_attrs[TERRA_BSDF_MAX_ATTRIBUTES];
    TerraFloat3   attrs[TERRA_MATERIAL_ATTRIBUTES];
} TerraShadingSurface;

//
// Samplers
//
// Adapted using the author's implementation as in
// http://www.pcg-random.org/
typedef struct TerraSamplerRandom {
    uint64_t state;
    uint64_t inc;
} TerraSamplerRandom;

// 2D Sampler
typedef struct TerraSamplerStratified {
    TerraSamplerRandom* random_sampler;
    int samples;
    int strata;
    int next;
    float stratum_size;
} TerraSamplerStratified;

// 2D Sampler
typedef struct TerraSamplerHalton {
    int next;
    int bases[2];
} TerraSamplerHalton;

// Sampler Interface
typedef void* TerraSampler;
typedef void ( *TerraSamplingRoutine ) ( void* sampler, float* e1, float* e2 );

typedef struct TerraSampler2D {
    TerraSampler         sampler;
    TerraSamplingRoutine sample;
} TerraSampler2D;

//
// Terra rendering internals
//
TerraRay    terra_camera_ray            ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height, const TerraFloat4x4* rot_opt );
bool        terra_find_closest_prim     ( struct TerraScene* scene, const TerraRay* ray, TerraPrimitiveRef* ref, TerraFloat3* intersection_point );
bool        terra_find_closest          ( struct TerraScene* scene, const TerraRay* ray, const TerraMaterial** material_out, TerraShadingSurface* surface_out, TerraFloat3* intersection_point );
TerraFloat3 terra_trace                 ( struct TerraScene* scene, const TerraRay* primary_ray );
TerraFloat3 terra_pixel_dir             ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height );
void        terra_triangle_init_shading ( const TerraTriangle* triangle, const TerraMaterial* material, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingSurface* surface );

//
// RNG internals
//
void  terra_sampler_random_init ( TerraSamplerRandom* sampler );
void  terra_sampler_random_destroy ( TerraSamplerRandom* sampler );
float terra_sampler_random_next ( void* sampler );
//void  terra_sampler_random_next_pair ( void* sampler, float* e1, float* e2 );

void  terra_sampler_stratified_init ( TerraSamplerStratified* sampler, TerraSamplerRandom* random_sampler, size_t strata_per_dimension, size_t samples_per_stratum );
void  terra_sampler_stratified_destroy ( TerraSamplerStratified* sampler );
void  terra_sampler_stratified_next_pair ( void* sampler, float* e1, float* e2 );

void  terra_sampler_halton_init ( TerraSamplerHalton* sampler );
void  terra_sampler_halton_destroy ( TerraSamplerHalton* sampler );
void  terra_sampler_halton_next_pair ( void* sampler, float* e1, float* e2 );


//
// Attributes
//
extern const TerraFloat3 kTerraMaterialDefaultIor;
#define TERRA_ATTR_CONSTANT_FLAG -1

typedef enum {
    TerraAttributeIor        = 0,
    TerraAttributeEmissive   = 1,
    TerraAttributeBumpMap    = 2,
    TerraAttributeNormalMap  = 3,
    TerraAttributeCount
} TerraAttributeType;

typedef enum {
    TerraMaterialFlagBumpMap    = 1 << TerraAttributeBumpMap,
    TerraMaterialFlagNormalMap  = 1 << TerraAttributeNormalMap
} TerraMaterialFlags;
TerraFloat3 terra_attribute_eval ( const TerraAttribute* attr, const void* addr );

//
// Textures
//
typedef union {
    uint8_t* p;
    float*   fp;
} TerraMapPtr;

typedef struct TerraMap { // Forward declarable
    TerraMapPtr data;
    uint16_t width;
    uint16_t height;
} TerraMap;

TerraAttributeEval terra_texture_sampler ( const TerraTexture* texture );
#define TERRA_TEXTURE_SAMPLER(name) TerraFloat3 terra_texture_sampler_##name ( intptr_t state, const void* addr )
TERRA_TEXTURE_SAMPLER ( mip0 );
TERRA_TEXTURE_SAMPLER ( mipmaps );
TERRA_TEXTURE_SAMPLER ( anisotropic );
TERRA_TEXTURE_SAMPLER ( spherical );

#ifdef __cplusplus
}
#endif

#endif
