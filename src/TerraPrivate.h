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

// Ray/primitive intersections return a 32-bit index which can be used to access
// the object and its triangle aswell as the specific intersection point.
typedef struct {
    // Barycentric coordinates and ray depth (U, V, W, Z)
    float u;
    float v;
    float w;
    float ray_depth;

    TerraFloat3 point;             // Intersection point in world coordinates

    uint32_t    object_idx : 8;    // Reference index to the model
    uint32_t    triangle_idx : 24; // Reference index to the triangle
} TerraRayIntersectionResult;

// Query of the
typedef struct {
    TerraRay*      ray;
    TerraRayState* state;
    union {
        TerraAABB      box;
        TerraTriangle* triangle;
    } primitive;
} TerraRayIntersectionQuery;

// Internal api
void  terra_sampler_random_init ( TerraSamplerRandom* sampler );
void  terra_sampler_random_destroy ( TerraSamplerRandom* sampler );
float terra_sampler_random_next ( void* sampler );

void  terra_sampler_stratified_init ( TerraSamplerStratified* sampler, TerraSamplerRandom* random_sampler, size_t strata_per_dimension, size_t samples_per_stratum );
void  terra_sampler_stratified_destroy ( TerraSamplerStratified* sampler );
void  terra_sampler_stratified_next_pair ( void* sampler, float* e1, float* e2 );

void  terra_sampler_halton_init ( TerraSamplerHalton* sampler );
void  terra_sampler_halton_destroy ( TerraSamplerHalton* sampler );
void  terra_sampler_halton_next_pair ( void* sampler, float* e1, float* e2 );

// Arbitrary discrete probability distribution sampling

// TODO use Vose's algorithm for O(1) time generation performance.
// http://www.keithschwarz.com/darts-dice-coins/
typedef struct {
    float* f;           // The function evaluated on its domain
    size_t n;           // The domain size (0->n-1)
    float* cdf;         // The function's cdf
    float  integral;     // The function's integral's value
} TerraDistribution1D;

typedef struct {
    TerraDistribution1D  marginal;      // Probability distribution of picking each row
    TerraDistribution1D* conditionals;  // Probability distribution of picking a value, given each row
} TerraDistributon2D;

void        terra_distribution_1d_init   ( TerraDistribution1D* dist, const float* f, size_t size );
float       terra_distribution_1d_sample ( TerraDistribution1D* dist, float e, float* pdf, size_t* idx );

void        terra_distribution_2d_init   ( TerraDistributon2D* dist, const float* f, size_t width, size_t height );
TerraFloat2 terra_distribution_2d_sample ( TerraDistributon2D* dist, float e1, float e2, float* pdf );

// TerraGeometry.c
int terra_geom_ray_triangle_intersection_init  ( const TerraRay* ray, TerraRayState* state );
int terra_geom_ray_triangle_intersection_query ( const TerraRayIntersectionQuery* query, TerraRayIntersectionResult* result );
int terra_geom_ray_box_intersection_init       ( const TerraRay* ray, TerraRayState* state );
int terra_geom_ray_box_intersection_query      ( const TerraRayIntersectionQuery* query, TerraRayIntersectionResult* result );

bool  terra_ray_aabb_intersection ( const TerraRay* ray, const TerraAABB* aabb, float* tmin_out, float* tmax_out );
void  terra_aabb_fit_triangle ( TerraAABB* aabb, const TerraTriangle* triangle );
float terra_triangle_area ( const TerraTriangle* triangle );

// TerraSpatialAcceleration.c
HTerraSpatialAcceleration terra_spatial_acceleration_create  ( HTerraScene scene );
void                      terra_spatial_acceleration_destroy ( HTerraSpatialAcceleration accel );
int                       terra_spatial_acceleration_query   ( HTerraSpatialAcceleration accel, const TerraRay* ray, TerraRayIntersectionResult* result );

#endif