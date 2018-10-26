#ifndef _TERRA_PRIVATE_H_
#define _TERRA_PRIVATE_H_

// Terra
#include <Terra.h>

// stdlib
#include <stdint.h>

// Uniform distribution sampling

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
typedef void ( *TerraSamplingRoutine ) ( TerraSampler sampler, float* e1, float* e2 );

typedef struct TerraSampler2D {
    TerraSampler         sampler;
    TerraSamplingRoutine sample;
} TerraSampler2D;

// API

void  terra_sampler_random_init ( TerraSamplerRandom* sampler );
void  terra_sampler_random_destroy ( TerraSamplerRandom* sampler );
float terra_sampler_random_next ( void* sampler );

void  terra_sampler_stratified_init ( TerraSamplerStratified* sampler, TerraSamplerRandom* random_sampler, size_t strata_per_dimension, size_t samples_per_stratum );
void  terra_sampler_stratified_destroy ( TerraSamplerStratified* sampler );
void  terra_sampler_stratified_next_pair ( void* sampler, float* e1, float* e2 );

void  terra_sampler_halton_init ( TerraSamplerHalton* sampler );
void  terra_sampler_halton_destroy ( TerraSamplerHalton* sampler );
void  terra_sampler_halton_next_pair ( void* sampler, float* e1, float* e2 );

// Arbitrary piecewise distribution sampling

// TODO use Vose's algorithm for O(1) time generation performance.
// http://www.keithschwarz.com/darts-dice-coins/
typedef struct {
    float* f;
    size_t n;
    float* cdf;
    float integral;
} TerraDistribution1D;

typedef struct {
    TerraDistribution1D* conditionals;
    TerraDistribution1D  marginal;
} TerraDistributon2D;

void        terra_distribution_1d_init ( TerraDistribution1D* dist, float* f, size_t n );
float       terra_distribution_1d_sample ( TerraDistribution1D* dist, float e, float* pdf, size_t* idx );

void        terra_distribution_2d_init ( TerraDistributon2D* dist, float* f, size_t nx, size_t ny );
TerraFloat2 terra_distribution_2d_sample ( TerraDistributon2D* dist, float e1, float e2, float* pdf );

#endif