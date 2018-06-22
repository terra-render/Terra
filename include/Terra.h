#ifndef _TERRA_H_
#define _TERRA_H_

// std
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <float.h>
#include <assert.h>
#include <stdint.h>

// Terra
#include "TerraMath.h"

#ifdef __cplusplus
extern "C" {
#endif


//
// Public data structures
//

// Built-in terra attributes. Used for ior, emissive, normal map, bumpmap, ..
// Indices are defined in TerraPrivate.h, but the routines are public
#define TERRA_MATERIAL_ATTRIBUTES 4

// Maximum number of material parameters usable by TerraBSDF.
// Each BSDF defines its own indices, see TerraPresets.h for examples
#ifndef TERRA_BSDF_MAX_ATTRIBUTES
#define TERRA_BSDF_MAX_ATTRIBUTES 8
#endif

//
// Shading context information used by the BSDF routines ( no need to include TerraPrivate.h)
// It's compiled and attributes are evaluated after detecting the collision
typedef struct { // Forward declarable
    TerraFloat4x4 rot;
    TerraFloat3   normal;
    TerraFloat2   uv;
    TerraFloat3   bsdf_attrs[TERRA_BSDF_MAX_ATTRIBUTES];
    TerraFloat3   attrs[TERRA_MATERIAL_ATTRIBUTES];
} TerraShadingSurface;

//
// Texture
//
// Texture sampling options
typedef enum {
    kTerraSamplerDefault     = 0,      // non-mipmapped point sampling
    kTerraSamplerBilinear    = 1,      // bilinear sampling
    kTerraSamplerTrilinear   = 1 << 1, // isotropic mipmaps (requires ray differentials)
    kTerraSamplerAnisotropic = 1 << 2, // anisotropic mipmaps (requires ray differentials) (includes TerraTrilinear)
    kTerraSamplerSpherical   = 1 << 3, // texture lookup using spherical coordinates as <u v>
    kTerraSamplerSRGB        = 1 << 4  // the provided texture is in sRGB (gets linearized at creation). sRGB ICC Profile http://color.org/chardata/rgb/sRGB.pdf
} TerraSamplerFlags;

// Addressing policy for out of range texture coordinates
typedef enum {
    kTerraTextureAddressWrap = 0,
    kTerraTextureAddressMirror,
    kTerraTextureAddressClamp,
} TerraTextureAddress;

// Texture handle.
// Textures are immutable objects. After initialization
// The definition is public for easier storage, but routines are present for
// manipulation:
// Creation:    terra_texture_create()      unsigned char  32bpp
//              terra_texture_create_fp()   floating point 16Bpp
// Destruction: terra_texture_destroy()
// The internal data layout is not guaranteed to match the one used as initial data.
typedef struct {
    uint16_t          width;        // Original width in pixels
    uint16_t          height;       // Original height in pixels
    uint8_t           sampler;      // TerraSamplerFlags
    uint8_t           depth    : 4; // size in bytes of each channel
    uint8_t           address  : 4; // TerraTextureAddress
    struct TerraMap*  mipmaps;      // Isotropic mipmaps internal data
    struct TerraMap*  ripmaps;      // Anisotropic mipmaps internal data
} TerraTexture;

//
// BSDF
//
// BSDF routine entry points
typedef TerraFloat3 ( TerraBSDFSampleRoutine ) ( const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo );
typedef float       ( TerraBSDFPdfRoutine )    ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo );
typedef TerraFloat3 ( TerraBSDFEvalRoutine )   ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo );

// BSDF definition
// See TerraPreset.h for bundled implementations
typedef size_t TerraBSDFAttribute; // Used for readability see `TerraPreset.h`
typedef struct {
    TerraBSDFSampleRoutine* sample;
    TerraBSDFPdfRoutine*    pdf;
    TerraBSDFEvalRoutine*   eval;
    size_t                  attrs_count;
} TerraBSDF;

//
// Material
//
// Attributes can either represent a constant value or a function evaluation such
// as sampling a texture or generating a dynamic value.
// Initialization routines are provided:
//        terra_attribute_init_constant(TerraAttribute*, TerraFloat4 val)
//        terra_attribute_init_texture(TerraAttribute*, TerraTexture*)
//
// Some more procedural attributes are contained in TerraPresets.h
//
// When working with textures, <addr> should correspond to the interpolated uv
// coordinates(TerraFloat2*) except if the sampler is spherical, in which case it
// is the view direction (TerraFloat3*).
typedef TerraFloat3 ( *TerraAttributeEval ) ( intptr_t state, const void* addr );
typedef struct {
    TerraAttributeEval eval;  // Cannot be null
    intptr_t           state; // any pointer or value stored by _attribute_init_*()
} TerraAttributeFn;

typedef struct {
    uint32_t    _flag;    // Used for paddding and Constant/Fn identification
    TerraFloat3 constant; // Value
} TerraAttributeConstant;

typedef union {
    union {
        TerraAttributeConstant constant;
        TerraAttributeFn       fn;
    };
} TerraAttribute;


// Material definition.
// The definition is public for easier storage, but routines are present for
// manipulation:
// - Construct:            terra_material_init(TerraMaterial*)
// - Set BSDF              terra_bsdf_init_* see `TerraPresets.h`
// - Set BSDF parameters:  terra_material_bsdf_attr(TerraMaterial*, int, TerraAttribute*)
// - Set other parameters: terra_material_attr_ior(TerraMaterial*, TerraAttribute*)
//                         terra_material_attr_emissive(TerraMaterial*, TerraAttribute*)
//                         terra_material_attr_bump_map(TerraMaterial*, TerraAttribute*)
//                         terra_material_attr_normal_map(TerraMaterial*, TerraAttribute*)
//                         ..
// See the public API below for the updated list of available `terra_material_attr_*`
typedef struct {
    TerraBSDF      bsdf;
    TerraAttribute bsdf_attrs[TERRA_BSDF_MAX_ATTRIBUTES];
    TerraAttribute attrs[TERRA_MATERIAL_ATTRIBUTES];
    uint32_t       _flags;
} TerraMaterial;

//
// Triangular mesh
//
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


//
// Renderable model
//
typedef struct {
    TerraTriangle*           triangles;
    TerraTriangleProperties* properties;
    size_t                   triangles_count;
    TerraMaterial            material;
} TerraObject;

//
// Rendering options
//
// Dynamic range compression algorithm used by `terra_tonemap`.
// For comparisons and more operators, see https://github.com/tizian/tonemapper
typedef enum {
    kTerraTonemapOpLinear,    // Pixel values remain in linear space
    kTerraTonemapOpSRGB,      // Conversion to SRGB. ICC Profile http://color.org/chardata/rgb/sRGB.pdf
    kTerraTonemapOpReinhard,  // Global Reinhard operator without autoregion dodging/burning http://www.cmap.polytechnique.fr/%7Epeyre/cours/x2005signal/hdr_photographic.pdf
    kTerraTonemapOpFilmic2,   // Graham Aldridge's extension to Naughty's dog filmic operator http://iwasbeingirony.blogspot.com/2010/04/approximating-film-with-tonemapping.html
    kTerraTonemapOpInsomniac  // http://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2012/09/an-efficient-and-user-friendly-tone-mapping-operator.pdf
} TerraTonemapOp;

//
// Collision detection
typedef enum {
    kTerraAcceleratorBVH,
    kTerraAcceleratorKDTree
} TerraAccelerator;

//
// Sampling method to be used for the hemisphere on first bounce
//
typedef enum {
    kTerraSamplerRandom,
    kTerraSamplerStratified,
    kTerraSamplerHalton,
    kTerraSamplerHammersley,
} TerraSamplingMethod;

//
// Antialiasing (supersampling) options
// Note that some supersampling methods require the number of samples per pixel
// to be a multiple of a specific number to produce optimal results. You can force
// the number of samples to be overwitten in `terra_scene_commit(.., adjust_samples)`
// You can then read the number of samples being used from `terra_scene_get_options`
//
// `TerraAAPattern` controls how subsequent rays are traced.
typedef enum {
    kTerraAAPatternNone,            // Every ray passes through the pixel's center
    kTerraAAPatternRandom,          // Randomly generated samples
    kTerraAAPatternStratified,      // Random sample in grid cell region
    kTerraAAPatternHalton,          // Sample points are generated from a low discrepancy seq
    kTerraAAPatternHammerseley,     // Sample points are generated from a low discrepancy seq
    kTerraAAPatternGrid,            // (Ordered grid) Uniformly distributed samples
    kTerraAAPatternPoissonDisc,     // Random low-frequency samples
    kTerraAAPatternCount
} TerraAAPattern;

//
// `TerraAAFlags` enables extra control over the samples
typedef enum {
    kTerraDefault    = 0,     // Samples are uniformely weighted (box filter)
    kTerraAAWeighted = 1,     // Weights samples with a normalized gaussian kernel
    kTerraAAAdaptive = 1 << 1 // Adapts sample patterns to pixel subregions with higher variance. Does not adjust the number of samples.
} TerraAAFlags;

//
// Rendering options for a single scene.
// Options are finalized at `terra_scene_commit`. Subsequent edits won't affect
// the rendering until another commit is done.
// See the respective types and functions for documentation and usage.
typedef struct {
    TerraAccelerator    accelerator;
    TerraSamplingMethod sampling_method;
    TerraAAPattern      antialiasing_pattern;
    TerraAAFlags        antialiasing_flags;
    TerraAttribute      environment_map;
    size_t              samples_per_pixel;
    size_t              bounces;
    float               manual_exposure;
    float               gamma;
    bool                direct_sampling;
} TerraSceneOptions;

typedef struct {
    uint32_t object_idx : 8;
    uint32_t triangle_idx : 24;
} TerraPrimitiveRef;

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

//--------------------------------------------------------------------------------------------------
// Terra public API
// Documentation and implementation can be found in Terra.c
//--------------------------------------------------------------------------------------------------
typedef void*   HTerraScene;

// Rendering
void                terra_render                    ( const TerraCamera* camera, HTerraScene scene, const TerraFramebuffer* framebuffer, size_t x, size_t y, size_t width, size_t height );
void                terra_tonemap                   ( TerraFloat3* pixels, TerraTonemapOp op );
bool                terra_pick                      ( TerraPrimitiveRef* prim, const TerraCamera* camera, HTerraScene scene, size_t x, size_t y, size_t width, size_t height );

// Scene
HTerraScene         terra_scene_create              ();
TerraObject*        terra_scene_add_object          ( HTerraScene scene, size_t triangle_count );
size_t              terra_scene_count_objects       ( HTerraScene scene );
void                terra_scene_commit              ( HTerraScene scene );
void                terra_scene_clear               ( HTerraScene scene );
TerraSceneOptions*  terra_scene_get_options         ( HTerraScene scene );
void                terra_scene_destroy             ( HTerraScene scene );
TerraObject*        terra_scene_object              ( HTerraScene scene, uint32_t object_idx ) ;

// Framebuffer
bool                terra_framebuffer_create        ( TerraFramebuffer* framebuffer, size_t width, size_t height );
void                terra_framebuffer_clear         ( TerraFramebuffer* framebuffer, const TerraFloat3* value );
void                terra_framebuffer_destroy       ( TerraFramebuffer* framebuffer );

// Textures
bool                terra_texture_create            ( TerraTexture* texture, const uint8_t* data, size_t width, size_t height, size_t components, size_t sampler, TerraTextureAddress address );
bool                terra_texture_create_fp         ( TerraTexture* texture, const float* data, size_t width, size_t height, size_t components, size_t sampler, TerraTextureAddress address );
void                terra_texture_destroy           ( TerraTexture* texture );

// Attributes
void                terra_attribute_init_constant   ( TerraAttribute* attr, const TerraFloat3* constant );
void                terra_attribute_init_texture    ( TerraAttribute* attr, TerraTexture* texture );
bool                terra_attribute_is_constant     ( const TerraAttribute* attr );

// Materials
void                terra_material_init             ( TerraMaterial* material );
void                terra_material_bsdf_attr        ( TerraMaterial* material, TerraBSDFAttribute bsdf_attr, const TerraAttribute* attr );
void                terra_material_ior              ( TerraMaterial* material, const TerraAttribute* attr );
void                terra_material_emissive         ( TerraMaterial* material, const TerraAttribute* attr );
void                terra_material_bump_map         ( TerraMaterial* material, const TerraAttribute* attr );
void                terra_material_normal_map       ( TerraMaterial* material, const TerraAttribute* attr );

#ifdef __cplusplus
}
#endif

#endif // _TERRA_H_