#ifndef _TERRA_PRIVATE_H_
#define _TERRA_PRIVATE_H_

// Terra public header
#include <Terra.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// Terra private API
// Implementation and documentation for the different modules is contained in the respective source file.
//--------------------------------------------------------------------------------------------------

// Let's avoid unnecessary includes as they are unused by most modules,
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
// Terra rendering internals (Terra.c)
//
extern const TerraFloat3 kTerraMaterialDefaultIor;
#define TERRA_ATTR_CONSTANT_FLAG -1

// Indices to TerraMaterial::attrs[]
typedef enum {
    TerraAttributeIor = 0,
    TerraAttributeEmissive = 1,
    TerraAttributeBumpMap = 2,
    TerraAttributeNormalMap = 3,
    TerraAttributeCount
} TerraAttributeType;

// Internal state stored in TerraMaterial::_flags
typedef enum {
    TerraMaterialFlagBumpMap = 1 << TerraAttributeBumpMap,
    TerraMaterialFlagNormalMap = 1 << TerraAttributeNormalMap
} TerraMaterialFlags;

typedef struct {
    TerraFloat3 center;
    float       radius;
    TerraAABB   aabb;
    float       emissive;
} TerraLight;

// Attributes
TerraFloat3 terra_attribute_eval ( const TerraAttribute* attr, const void* addr );

// Raytracing
void        terra_ray_create ( TerraRay* ray, TerraFloat3* point, TerraFloat3* direction, TerraFloat3* normal, float sign );
TerraFloat3 terra_pixel_dir ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height );
TerraRay    terra_camera_ray ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height, const TerraFloat4x4* rot_opt );

// Collisions
bool        terra_ray_triangle_intersection ( const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out, float* t_out );
bool        terra_ray_aabb_intersection ( const TerraRay* ray, const TerraAABB* aabb, float* tmin_out, float* tmax_out );
void        terra_aabb_fit_triangle ( TerraAABB* aabb, const TerraTriangle* triangle );
bool        terra_find_closest_prim ( struct TerraScene* scene, const TerraRay* ray, TerraPrimitiveRef* ref, TerraFloat3* intersection_point );
bool        terra_find_closest ( struct TerraScene* scene, const TerraRay* ray, const TerraMaterial** material_out, TerraShadingSurface* surface_out, TerraFloat3* intersection_point );

// Shading
void        terra_build_rotation_around_normal ( TerraShadingSurface* surface );
void        terra_triangle_init_shading ( const TerraTriangle* triangle, const TerraMaterial* material, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingSurface* surface );
TerraFloat3 terra_trace ( struct TerraScene* scene, const TerraRay* primary_ray );

// Direct Lighting
TerraLight* terra_light_pick_power_proportional ( const struct TerraScene* scene, float* e1 );
void        terra_lookatf4x4 ( TerraFloat4x4* mat_out, const TerraFloat3* normal );
TerraFloat3 terra_light_sample_disk ( const TerraLight* light, const TerraFloat3* surface_point, float e1, float e2 );

// System
void*       terra_malloc ( size_t size );
void*       terra_realloc ( void* ptr, size_t size );
void        terra_free ( void* ptr );
void        terra_log ( const char* str, ... );


//
// Samplers (TerraSamplers.c)
//
// All the samplers except for SamplerRandom produce only 2D results. The RandomSampler
// has 1D routines as it's used as foundation by the other samplers and in other routines too.
//
// Generates uniformly distributed real numbers using 64 bit PCG http://www.pcg-random.org/
// Adapted using the author's C implementation https://github.com/imneme/pcg-c
typedef struct {
    uint64_t state;
    uint64_t inc;
} TerraSamplerRandom;

typedef struct {
    float* samples;
    size_t next;
    size_t n_samples;
} TerraSampler2D;

// Routines
void  terra_sampler_random_init ( TerraSamplerRandom* sampler );
float terra_sampler_random_next ( TerraSamplerRandom* sampler );

void terra_sampler2d_random_init     ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* rng );
void terra_sampler2d_stratified_init ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* rng );
void terra_sampler2d_halton_init     ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* rng );
void terra_sampler2d_hammersley_init ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* rng );
void terra_sampler2d_next            ( TerraSampler2D* sampler, float* e1, float* e2 );
void terra_sampler2d_destroy         ( TerraSampler2D* sampler );

// Antialiasing pattern generators. All samples are in [0 1]^2
// When called with samples is NULL returns the minimum number of samples needed based on n_samples
size_t terra_pattern_random          ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng );
size_t terra_pattern_stratified      ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng );
size_t terra_pattern_halton          ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng );
size_t terra_pattern_hammersley      ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng );
size_t terra_pattern_grid            ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng );
size_t terra_pattern_poisson         ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng );

//
// Textures (TerraTextures.c)
//
// 2D buffer containing pixel data. Format information is in TerraTexture.
typedef struct TerraMap { // Forward declarable
    void*    data;
    uint16_t width;
    uint16_t height;
} TerraMap;

int                terra_mimaps_count       ( uint16_t w, uint16_t h );
int                terra_ripmaps_count      ( uint16_t w, uint16_t h );
TerraMap           terra_map_create_mip     ( uint16_t w, uint16_t h, const TerraMap* ptr );
TerraMap           terra_map_create_mip0    ( size_t w, size_t h, size_t components, size_t depth, const void* data );
void               terra_map_destroy        ( TerraMap* map );
bool               terra_texture_create_any ( TerraTexture* texture, const void* data, size_t width, size_t height, size_t depth, size_t components, size_t sampler, TerraTextureAddress address );
TerraAttributeEval terra_texture_sampler    ( const TerraTexture* texture );

#define TERRA_TEXTURE_SAMPLER(name) TerraFloat3 terra_texture_sampler_##name ( intptr_t state, const void* addr )
TERRA_TEXTURE_SAMPLER ( mip0 );
TERRA_TEXTURE_SAMPLER ( mipmaps );
TERRA_TEXTURE_SAMPLER ( anisotropic );
TERRA_TEXTURE_SAMPLER ( spherical );

#ifdef __cplusplus
}
#endif

#endif
