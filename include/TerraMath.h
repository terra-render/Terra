#ifndef _TERRA_MATH_H_
#define _TERRA_MATH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Math / Basic Types
//--------------------------------------------------------------------------------------------------
#define TERRA_PI 3.1416926535f
#define TERRA_2PI 6.283185307f
#define TERRA_EPS (float)1e-5

typedef struct {
    uint32_t x, y;
} TerraUInt2;

// 2D vector
typedef struct {
    float x, y;
} TerraFloat2;

// 3D vector
typedef struct {
    float x, y, z;
} TerraFloat3;

// 4D vector
typedef struct {
    float x, y, z, w;
} TerraFloat4;

// Matrix
typedef struct {
    TerraFloat4 rows[4];
} TerraFloat4x4;

// Ray
typedef struct {
    TerraFloat3 origin;
    TerraFloat3 direction;
    TerraFloat3 inv_direction;
} TerraRay;

typedef struct {
    TerraFloat3 min;
    TerraFloat3 max;
} TerraAABB;

extern const TerraFloat3 TERRA_F3_ZERO;
extern const TerraFloat3 TERRA_F3_ONE;

//--------------------------------------------------------------------------------------------------
// Math functions
//--------------------------------------------------------------------------------------------------
static inline TerraUInt2  terra_ui2_set ( uint32_t x, uint32_t y );
static inline TerraFloat2 terra_f2_set ( float x, float y );
static inline TerraFloat3 terra_f3_set ( float x, float y, float z );
static inline TerraFloat3 terra_f3_set1 ( float xyz );
static inline TerraFloat3 terra_f3_ptr ( float* d );
static inline TerraFloat4 terra_f4 ( float x, float y, float z, float w );
static inline TerraFloat4 terra_f4_ptr ( float* d );
static inline bool        terra_equalf3 ( TerraFloat3* a, TerraFloat3* b );
static inline TerraFloat3 terra_addf3 ( const TerraFloat3* left, const TerraFloat3* right );
static inline TerraFloat2 terra_addf2 ( const TerraFloat2* left, const TerraFloat2* right );
static inline TerraFloat3 terra_subf3 ( const TerraFloat3* left, const TerraFloat3* right );
static inline TerraFloat2 terra_mulf2 ( const TerraFloat2* left, float scale );
static inline TerraFloat3 terra_mulf3 ( const TerraFloat3* vec, float scale );
static inline TerraFloat3 terra_powf3 ( const TerraFloat3* vec, float exp );
static inline TerraFloat3 terra_pointf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3 terra_divf3 ( const TerraFloat3* vec, float val );
static inline TerraFloat3 terra_divf3c ( const TerraFloat3* a, const TerraFloat3* b );
static inline float       terra_dotf2 ( const TerraFloat2* a, const TerraFloat2* b );
static inline float       terra_dotf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3 terra_crossf3 ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3 terra_negf3 ( const TerraFloat3* vec );
static inline float       terra_lenf3 ( const TerraFloat3* vec );
static inline float       terra_distsq ( const TerraFloat3* a, const TerraFloat3* b );
static inline TerraFloat3 terra_normf3 ( const TerraFloat3* vec );
static inline TerraFloat3 terra_transformf3 ( const TerraFloat4x4* transform, const TerraFloat3* vec );
static inline float       terra_lerp ( float a, float b, float t );
static inline TerraFloat3 terra_lerpf3 ( const TerraFloat3* a, const TerraFloat3* b, float t );
static inline float       terra_absf ( float a );
static inline size_t      terra_clampi ( size_t val, size_t low, size_t high );
static inline float       terra_radians ( float degrees );
static inline uint64_t    terra_next_pow2sq ( uint64_t v );
static inline float       terra_radical_inverse ( uint64_t base, uint64_t a );
static inline bool        terra_is_integer ( float v );

#ifdef __cplusplus
}
#endif


#include "TerraMath.inl"

#endif // _TERRA_MATH_H_