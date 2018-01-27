#ifndef _TERRA_PRESETS_H_
#define _TERRA_PRESETS_H_

// Terra
#include "Terra.h"

void terra_bsdf_init_diffuse(TerraBSDF* bsdf);
void terra_bsdf_init_rough_dielectric(TerraBSDF* bsdf);
void terra_bsdf_init_blinn_phong(TerraBSDF* bsdf);
void terra_bsdf_init_glass(TerraBSDF* bsdf);


#endif // _TERRA_PRESETS_H_