#ifndef _TERRA_MATH_H_
#define _TERRA_MATH_H_

// libc
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

// Matrices are stored row-major (in 4 adjacent 4-float arrays)
// Vectors are columns, matrix vector multiplication is Mv
// Coordinate system is right handed: x right, y up, z out.

//--------------------------------------------------------------------------------------------------
// Math / Basic Types
//--------------------------------------------------------------------------------------------------
#define terra_PI 3.1416926535f
#define terra_Epsilon 1e-4

typedef struct TerraInt4 {
    int x, y, z, w;
} TerraInt4;

typedef struct TerraFloat2 {
    float x, y;
} TerraFloat2;

typedef struct TerraFloat3 {
    float x, y, z;
} TerraFloat3;

typedef struct TerraFloat4 {
    float x, y, z, w;
} TerraFloat4;

typedef struct TerraFloat4x4 {
    TerraFloat4 rows[4];
} TerraFloat4x4;

#define terra_ior_air 1.f
#define terra_f2_zero terra_f2_set(0.f, 0.f)
#define terra_f3_zero terra_f3_set1(0.f)
#define terra_f3_one terra_f3_set1(1.f)

//--------------------------------------------------------------------------------------------------
// Math functions
//--------------------------------------------------------------------------------------------------
static inline TerraFloat2   terra_f2_set ( float x, float y );
static inline TerraFloat3   terra_f3_set ( float x, float y, float z );
static inline TerraFloat3   terra_f3_set1 ( float xyz );
static inline TerraFloat3   terra_f3_setv ( const float* xyz );
static inline TerraFloat4   terra_f4_set ( float x, float y, float z, float w );
static inline TerraInt4     terra_i4_set ( int x, int y, int z, int w );
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
static inline float         terra_sqlenf3 ( const TerraFloat3* vec );
static inline float         terra_distf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline float         terra_sqdistf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3   terra_normf3 ( const TerraFloat3* vec );
static inline int           terra_max_coefff3 ( const TerraFloat3* vec );
static inline float         terra_maxf ( float a, float b );
static inline float         terra_minf ( float a, float b );
static inline size_t        terra_maxi ( size_t a, size_t b );
static inline size_t        terra_mini ( size_t a, size_t b );
static inline int           terra_signf ( float v );
static inline uint32_t      terra_signf_mask ( float v );
static inline float         terra_maxf3 ( const TerraFloat3* vec );
static inline float         terra_min3 ( const TerraFloat3* vec );
static inline TerraFloat3   terra_absf3 ( const TerraFloat3* vec );
static inline float         terra_xorf ( float lhs, float rhs );
static inline void          terra_swap_xorf ( float* a, float* b );
static inline void          terra_swap_xori ( int* a, int* b );
static inline TerraFloat3   terra_transformf3 ( const TerraFloat4x4* transform, const TerraFloat3* vec );
static inline bool          terra_f3_is_zero ( const TerraFloat3* f3 );
static inline float         terra_lerp ( float a, float b, float t );
static inline TerraFloat3   terra_lerpf3 ( const TerraFloat3* a, const TerraFloat3* b, float t );
static inline TerraFloat4x4 terra_f4x4_from_y ( const TerraFloat3* y_axis );
static inline TerraFloat3   terra_clampf3 ( const TerraFloat3* f3, const TerraFloat3* min, const TerraFloat3* max );

#include "TerraMath.inl"

#endif // _TERRA_MATH_H_