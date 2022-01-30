#ifndef _TERRA_PRESETS_H_
#define _TERRA_PRESETS_H_

// Terra
#include "Terra.h"

#ifdef __cplusplus
extern "C" {
#endif

// DIFFUSE
#define TERRA_DIFFUSE_ALBEDO    0
#define TERRA_DIFFUSE_END       1
void terra_bsdf_diffuse_init ( TerraBSDF* bsdf );

typedef struct {
    TerraFloat3 albedo;
} TerraMaterialDiffuse;

// PHONG
// attribute float3: <ks, kd, intensity> ?
#define TERRA_PHONG_ALBEDO             1
#define TERRA_PHONG_SPECULAR_COLOR     0
#define TERRA_PHONG_SPECULAR_INTENSITY 2
#define TERRA_PHONG_SAMPLE_PICK        3
#define TERRA_PHONG_END                4
void terra_bsdf_phong_init ( TerraBSDF* bsdf );

typedef struct {
    TerraFloat3 albedo;
    TerraFloat3 specular_color;
    float specular_intensity;
    bool sample_diffuse;
} TerraMaterialPhong;

#define TERRA_DISNEY_BASE_COLOR 0
#define TERRA_DISNEY_SHEEN      1

typedef struct {
    TerraFloat3 base_color;
    float specular;
    float specular_tint;
    float sheen;
    float sheen_tint;
    float clearcoat;
    float clearcoat_gloss;
    float metalness;
    float roughness;
    float anisotropic;
    float subsurface;
} TerraMaterialDisney;

// PROFILE
#define TERRA_PROFILE_SESSION_DEFAULT   0

#define TERRA_PROFILE_TARGET_RENDER     0
#define TERRA_PROFILE_TARGET_TRACE      1
#define TERRA_PROFILE_TARGET_RAY        2
#define TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION 3
#define TERRA_PROFILE_TARGET_COUNT      4

#ifdef __cplusplus
}
#endif

#endif