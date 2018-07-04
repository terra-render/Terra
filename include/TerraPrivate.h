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
#define TERRA_RESTRICT __restrict // almost..
#else
#define TERRA_ASSERT(cond) { if (!(cond)) { __builtin_trap();} }

#if __STDC_VERSION__ >= 199901L
#define TERRA_RESTRICT restrict
#else
#define TERRA_RESTRICT __restrict__
#error This won't likely work, maybe with many extensions...
#endif

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

typedef struct {
    TerraFloat3 pos_dx;
    TerraFloat3 pos_dy;
    TerraFloat3 dir_dx;
    TerraFloat3 dir_dy;
} TerraRayDifferentials;

// Attributes
TerraFloat3 terra_attribute_eval ( const TerraAttribute* attr, const void* addr );

// Raytracing
void        terra_ray_create                 ( TerraRay* ray, TerraFloat3* point, TerraFloat3* direction, TerraFloat3* normal, float sign );
TerraFloat3 terra_pixel_dir                  ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height );
TerraRay    terra_camera_ray                 ( const TerraCamera* camera, size_t x, size_t y, size_t width, size_t height );
void        terra_ray_differentials_transfer ( TerraRayDifferentials* differentials );
void        terra_ray_differentials_reflect  ( TerraRayDifferentials* differentials );
void        terra_ray_differentials_refract  ( TerraRayDifferentials* differentials );

// Collisions
bool        terra_ray_triangle_intersection ( const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out, float* t_out );
bool        terra_ray_aabb_intersection     ( const TerraRay* ray, const TerraAABB* aabb, float* tmin_out, float* tmax_out );
void        terra_aabb_fit_triangle         ( TerraAABB* aabb, const TerraTriangle* triangle );
bool        terra_find_closest_prim         ( struct TerraScene* scene, const TerraRay* ray, TerraPrimitiveRef* ref, TerraFloat3* intersection_point );
bool        terra_find_closest              ( struct TerraScene* scene, const TerraRay* ray, const TerraMaterial** material_out, TerraShadingSurface* surface_out, TerraFloat3* intersection_point );

// Shading
void        terra_build_rotation_around_normal ( TerraShadingSurface* surface );
void        terra_triangle_init_shading        ( const TerraTriangle* triangle, const TerraMaterial* material, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingSurface* surface );
TerraFloat3 terra_trace                        ( struct TerraScene* scene, const TerraRay* primary_ray, TerraFloat2* differentials );

// Direct Lighting
TerraLight* terra_light_pick_power_proportional ( const struct TerraScene* scene, float* e1 );
void        terra_lookatf4x4                    ( TerraFloat4x4* mat_out, const TerraFloat3* normal );
TerraFloat3 terra_light_sample_disk             ( const TerraLight* light, const TerraFloat3* surface_point, float e1, float e2 );

// System
void*       terra_malloc  ( size_t size );
void*       terra_realloc ( void* ptr, size_t size );
void        terra_free    ( void* ptr );
void        terra_log     ( const char* str, ... );


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
    float*   data;
    uint16_t width;
    uint16_t height;
} TerraMap;

size_t             terra_linear_index        ( uint16_t x, uint16_t y, uint16_t w );
size_t             terra_map_stride          ( const TerraMap* map );
void               terra_map_convolution     ( TerraMap* dst, const TerraMap* src, const float* kernel, size_t kernel_size );
size_t             terra_mips_count          ( uint16_t w, uint16_t h );
void               terra_mip_lvl_dims        ( uint16_t w, uint16_t h, int lvl, uint16_t* mip_w, uint16_t* mip_h );
size_t             terra_rips_count          ( uint16_t w, uint16_t h );
void               terra_map_init            ( TerraMap* map, uint16_t w, uint16_t h );
TerraMap           terra_map_create_mip0     ( size_t w, size_t h, size_t components, size_t depth, const void* data );
void               terra_map_destroy         ( TerraMap* map );
bool               terra_texture_create_any  ( TerraTexture* texture, const void* data, size_t width, size_t height, size_t depth, size_t components, size_t sampler, TerraTextureAddress address );
TerraAttributeEval terra_texture_sampler     ( const TerraTexture* texture );
size_t             terra_texture_mips_count  ( const TerraTexture* texture );
size_t             terra_texture_rips_count  ( const TerraTexture* texture );
TerraFloat3        terra_map_read_texel      ( const TerraMap* map, uint16_t x, uint16_t y );
void               terra_map_write_texel     ( TerraMap* map, uint16_t x, uint16_t y, const TerraFloat3* rgb );
void               terra_map_texel_int       ( const TerraMap* map, TerraTextureAddress address, const TerraFloat2* uv, uint16_t* x, uint16_t* y );
TerraFloat3        terra_map_sample_nearest  ( const TerraMap* map, const TerraFloat2* uv, TerraTextureAddress address );
TerraFloat3        terra_map_sample_bilinear ( const TerraMap* map, const TerraFloat2* _uv, TerraTextureAddress address );
void               terra_dpid_downscale      ( TerraMap* O, const TerraMap* I );

#define TERRA_TEXTURE_SAMPLER(name) TerraFloat3 terra_texture_sampler_##name ( intptr_t state, const void* addr )
TERRA_TEXTURE_SAMPLER ( mip0 );
TERRA_TEXTURE_SAMPLER ( mipmaps );
TERRA_TEXTURE_SAMPLER ( anisotropic );
TERRA_TEXTURE_SAMPLER ( spherical );

#ifdef __cplusplus
}
#endif

#endif
