#ifndef _TERRA_MATH_H_
#define _TERRA_MATH_H_

#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Math / Basic Types
//--------------------------------------------------------------------------------------------------
#define terra_PI 3.1416926535f
#define terra_PI2 6.283185307f
#define terra_Epsilon 0.0001f

// 2D vector
typedef struct TerraFloat2 {
    float x, y;
} TerraFloat2;

// 3D vector
typedef struct TerraFloat3 {
    float x, y, z;
} TerraFloat3;

// 4D vector
typedef struct TerraFloat4 {
    float x, y, z, w;
} TerraFloat4;

// Matrix
typedef struct TerraFloat4x4 {
    TerraFloat4 rows[4];
} TerraFloat4x4;

#define terra_ior_air 1.f
#define terra_f3_zero terra_f3_set1(0.f)
#define terra_f3_one terra_f3_set1(1.f)

//--------------------------------------------------------------------------------------------------
// Math functions
//--------------------------------------------------------------------------------------------------
static inline TerraFloat2   terra_f2_set ( float x, float y );
static inline TerraFloat3   terra_f3_set ( float x, float y, float z );
static inline TerraFloat3   terra_f3_set1 ( float xyz );
static inline TerraFloat4   terra_f4 ( float x, float y, float z, float w );
static inline bool          terra_equalf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3   terra_addf3 ( const TerraFloat3* left, const TerraFloat3* right );
static inline TerraFloat2   terra_addf2 ( const TerraFloat2* left, const TerraFloat2* right );
static inline TerraFloat3   terra_subf3 ( const TerraFloat3* left, const TerraFloat3* right );
static inline TerraFloat2   terra_mulf2 ( const TerraFloat2* left, float scale );
static inline TerraFloat3   terra_mulf3 ( const TerraFloat3* vec, float scale );
static inline TerraFloat3   terra_powf3 ( const TerraFloat3* vec, float exp );
static inline TerraFloat3   terra_pointf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3   terra_divf3 ( const TerraFloat3* vec, float val );
static inline float         terra_dotf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3   terra_crossf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3   terra_negf3 ( const TerraFloat3* vec );
static inline float         terra_lenf3 ( const TerraFloat3* vec );
static inline float         terra_distf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline float         terra_distance_squaredf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3   terra_normf3 ( const TerraFloat3* vec );
static inline float         terra_maxf ( float a, float b );
static inline float         terra_minf ( float a, float b );
static inline size_t        terra_maxi ( size_t a, size_t b );
static inline size_t        terra_mini ( size_t a, size_t b );
static inline float         terra_maxf3 ( const TerraFloat3* vec );
static inline float         terra_min3 ( const TerraFloat3* vec );
static inline void          terra_swapf ( float* a, float* b );
static inline TerraFloat3   terra_transformf3 ( const TerraFloat4x4* transform, const TerraFloat3* vec );
static inline bool          terra_f3_is_zero ( const TerraFloat3* f3 );
static inline float         terra_lerp ( float a, float b, float t );
static inline TerraFloat3   terra_lerpf3 ( const TerraFloat3* a, const TerraFloat3* b, float t );
static inline TerraFloat4x4 terra_f4x4_from_y ( const TerraFloat3* y_axis );

#include "TerraMath.inl"

#endif // _TERRA_MATH_H_