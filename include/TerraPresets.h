#ifndef _TERRA_PRESETS_H_
#define _TERRA_PRESETS_H_

// Terra
#include "Terra.h"

#define TERRA_DIFFUSE_ALBEDO 0
void terra_bsdf_init_diffuse(TerraBSDF* bsdf);

#define TERRA_PHONG_ALBEDO             0
#define TERRA_PHONG_SPECULAR_COLOR     1
#define TERRA_PHONG_SPECULAR_INTENSITY 2
void terra_bsdf_init_blinn_phong(TerraBSDF* bsdf);

void terra_bsdf_init_rough_dielectric(TerraBSDF* bsdf);

void terra_bsdf_init_glass(TerraBSDF* bsdf);


#endif // _TERRA_PRESETS_H_