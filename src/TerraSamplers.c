// Header
#include "TerraPrivate.h"

#include <stdio.h>

#pragma warning (disable : 4146)

//--------------------------------------------------------------------------------------------------
// Random
//--------------------------------------------------------------------------------------------------
void  terra_sampler_random_init ( TerraSamplerRandom* sampler ) {
    uint32_t seed = ( uint32_t ) ( ( uint64_t ) time ( NULL ) ^ ( uint64_t ) &exit );
    sampler->state = 0;
    sampler->inc = 1;
    terra_sampler_random_next ( sampler );
    sampler->state += seed;
    terra_sampler_random_next ( sampler );
}

float terra_sampler_random_next ( TerraSamplerRandom* sampler ) {
    uint64_t old_state = sampler->state;
    sampler->state = old_state * 6364136223846793005ULL + sampler->inc;
    uint32_t xorshifted = ( uint32_t ) ( ( ( old_state >> 18 ) ^ old_state ) >> 27 );
    uint32_t rot = ( uint32_t ) ( old_state >> 59 );
    uint32_t rndi = ( xorshifted >> rot ) | ( xorshifted << ( ( -rot ) & 31 ) );
    uint64_t max = ( uint64_t ) 1 << 32;
    float resolution = 1.f / max;
    return rndi * resolution;
}

void terra_sampler2d_random_init ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* rng ) {
    sampler->n_samples = n_samples;
    sampler->samples   = terra_malloc ( sizeof ( float ) * n_samples * 2 );
    sampler->next      = 0;

    for ( size_t i = 0; i < n_samples * 2; i += 2 ) {
        sampler->samples[i] = terra_sampler_random_next ( rng );
        sampler->samples[i + 1] = terra_sampler_random_next ( rng );
    }
}

//--------------------------------------------------------------------------------------------------
// Stratified
//--------------------------------------------------------------------------------------------------
void terra_sampler2d_stratified_init ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* rng ) {
    size_t expected = terra_next_pow2sq ( n_samples );
    n_samples = expected * expected;

    sampler->n_samples = n_samples;
    sampler->samples   = terra_malloc ( sizeof ( float ) * n_samples * 2 );
    sampler->next      = 0;

    float inv_expected = 1.f / expected;

    for ( size_t y = 0; y < expected; ++y ) {
        for ( size_t x = 0; x < expected; ++x ) {
            size_t idx = 2 * ( y * expected + x );
            sampler->samples[idx]     = terra_minf ( ( x + terra_sampler_random_next ( rng ) ) * inv_expected, 1.f - TERRA_EPS );
            sampler->samples[idx + 1] = terra_minf ( ( y + terra_sampler_random_next ( rng ) ) * inv_expected, 1.f - TERRA_EPS );
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Halton
//--------------------------------------------------------------------------------------------------
void terra_sampler2d_halton_init ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* _unused0 ) {
    size_t expected = terra_next_pow2sq ( n_samples );
    n_samples = expected * expected;

    sampler->n_samples = n_samples;
    sampler->samples   = ( float* ) terra_malloc ( sizeof ( float ) * n_samples * 2 );
    sampler->next      = 0;

    for ( size_t i = 0; i < n_samples * 2; i += 2 ) {
        sampler->samples[i]     = terra_radical_inverse ( 2, i / 2 );
        sampler->samples[i + 1] = terra_radical_inverse ( 3, i / 2 );
    }
}

//--------------------------------------------------------------------------------------------------
// Hammersley
//--------------------------------------------------------------------------------------------------
void terra_sampler2d_hammersley_init ( TerraSampler2D* sampler, size_t n_samples, TerraSamplerRandom* _unused0 ) {
    size_t expected = terra_next_pow2sq ( n_samples );
    n_samples = expected * expected;

    sampler->n_samples = n_samples;
    sampler->samples   = ( float* ) terra_malloc ( sizeof ( float ) * n_samples * 2 );
    sampler->next      = 0;

    const float inv_n_samples = 0.5f / n_samples;

    for ( size_t i = 0; i < n_samples * 2; i += 2 ) {
        sampler->samples[i] = ( float ) i * inv_n_samples;
        sampler->samples[i + 1] = terra_radical_inverse ( 2, i  / 2 );
    }
}

void terra_sampler2d_next ( TerraSampler2D* sampler, float* e1, float* e2 ) {
    *e1 = sampler->samples[sampler->next];
    *e2 = sampler->samples[sampler->next + 1];
    sampler->next += 2;
}

void terra_sampler2d_destroy ( TerraSampler2D* sampler ) {
    if ( sampler->samples ) {
        terra_free ( sampler->samples );
        sampler->samples = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
// AA Patterns
//--------------------------------------------------------------------------------------------------
#define TERRA_PATTERN_SAMPLER(name)\
    size_t terra_pattern_##name ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng ) {\
        if (samples == NULL) {\
            return n_samples;\
        }\
        TerraSampler2D gen;\
        terra_sampler2d_##name##_init(&gen, n_samples, rng);\
        for (size_t i = 0; i < n_samples; ++i) {\
            terra_sampler2d_next(&gen, &samples[i].x, &samples[i].y);\
        }\
        return n_samples;\
    }
// ---
TERRA_PATTERN_SAMPLER ( random );
TERRA_PATTERN_SAMPLER ( stratified );
TERRA_PATTERN_SAMPLER ( halton );
TERRA_PATTERN_SAMPLER ( hammersley );

// n_samples should a power of two squared
size_t terra_pattern_grid ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng ) {
    size_t expected = ( size_t ) terra_next_pow2sq ( ( uint64_t ) n_samples );
    n_samples = expected * expected;

    const float dim = 1.f / expected;

    // Placing at center of cell
    for ( size_t y = 0; y < expected; ++y ) {
        for ( size_t x = 0; x < expected; ++x ) {
            size_t idx = y * expected + x;
            samples[idx].x = dim * x + dim * .5f;
            samples[idx].y = dim * y + dim * .5f;
        }
    }

    return n_samples;
}

/*
    In order to generate exactly n_samples in a fixed domain [0 1]
    a NxN free mask is constructed

*/
size_t terra_pattern_poisson ( TerraFloat2* samples, size_t n_samples, TerraSamplerRandom* rng ) {
    if ( samples == NULL ) {
        return n_samples;
    }

    return n_samples;
}
