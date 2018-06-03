#ifndef _TERRA_PRIVATE_H_
#define _TERRA_PRIVATE_H_

// Terra
#include <Terra.h>

// stdlib
#include <stdint.h>

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

// Functions
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

#endif