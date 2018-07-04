#include "TerraMath.h"

#define TERRA_MAX(a, b) ((a) > (b) ? (a) : (b))
#define TERRA_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TERRA_CLAMP(v, l, h) TERRA_MAX((l), TERRA_MIN((v), (h)))

//--------------------------------------------------------------------------------------------------
// Math implementation
//--------------------------------------------------------------------------------------------------
inline TerraUInt2 terra_ui2_set ( uint32_t x, uint32_t y ) {
    TerraUInt2 ret;
    ret.x = x;
    ret.y = y;
    return ret;
}

inline TerraFloat2 terra_f2_set ( float x, float y ) {
    TerraFloat2 ret;
    ret.x = x;
    ret.y = y;
    return ret;
}

inline TerraFloat3 terra_f3_set ( float x, float y, float z ) {
    TerraFloat3 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    return ret;
}

inline TerraFloat3 terra_f3_set1 ( float xyz ) {
    TerraFloat3 ret;
    ret.x = ret.y = ret.z = xyz;
    return ret;
}

TerraFloat3 terra_f3_ptr ( float* d ) {
    return terra_f3_set ( d[0], d[1], d[2] );
}

inline TerraFloat4 terra_f4 ( float x, float y, float z, float w ) {
    TerraFloat4 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;
    return ret;
}

TerraFloat4 terra_f4_ptr ( float* d ) {
    return terra_f4 ( d[0], d[1], d[2], d[3] );
}

inline bool terra_equalf3 ( TerraFloat3* a, TerraFloat3* b ) {
    return ( a->x == b->x && a->y == b->y && a->z == b->z );
}

inline TerraFloat3 terra_addf3 ( const TerraFloat3* left, const TerraFloat3* right ) {
    return terra_f3_set (
               left->x + right->x,
               left->y + right->y,
               left->z + right->z );
}

inline TerraFloat2 terra_addf2 ( const TerraFloat2* left, const TerraFloat2* right ) {
    return terra_f2_set ( left->x + right->x, left->y + right->y );
}

inline TerraFloat3 terra_subf3 ( const TerraFloat3* left, const TerraFloat3* right ) {
    return terra_f3_set (
               left->x - right->x,
               left->y - right->y,
               left->z - right->z );
}

inline TerraFloat2 terra_mulf2 ( const TerraFloat2* left, float scale ) {
    return terra_f2_set ( left->x * scale, left->y * scale );
}

inline TerraFloat3 terra_mulf3 ( const TerraFloat3* vec, float scale ) {
    return terra_f3_set (
               vec->x * scale,
               vec->y * scale,
               vec->z * scale );
}

inline TerraFloat3 terra_powf3 ( const TerraFloat3* vec, float exp ) {
    return terra_f3_set (
               powf ( vec->x, exp ),
               powf ( vec->y, exp ),
               powf ( vec->z, exp )
           );
}

inline TerraFloat3 terra_pointf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
    return terra_f3_set ( a->x * b->x, a->y * b->y, a->z * b->z );
}

inline TerraFloat3 terra_divf3 ( const TerraFloat3* vec, float val ) {
    return terra_f3_set (
               vec->x / val,
               vec->y / val,
               vec->z / val );
}

inline TerraFloat3 terra_divf3c ( const TerraFloat3* a, const TerraFloat3* b ) {
    return terra_f3_set (
               a->x / b->x,
               a->y / b->y,
               a->z / b->z );
}

inline float terra_dotf2 ( const TerraFloat2* a, const TerraFloat2* b ) {
    return a->x * a->x + a->y * a->y;
}

inline float terra_dotf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

inline TerraFloat3 terra_crossf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
    return terra_f3_set (
               a->y * b->z - a->z * b->y,
               a->z * b->x - a->x * b->z,
               a->x * b->y - a->y * b->x );
}

inline TerraFloat3 terra_negf3 ( const TerraFloat3* vec ) {
    return terra_f3_set (
               -vec->x,
               -vec->y,
               -vec->z );
}

inline float terra_lenf3 ( const TerraFloat3* vec ) {
    return sqrtf (
               vec->x * vec->x +
               vec->y * vec->y +
               vec->z * vec->z );
}

inline float terra_distsq ( const TerraFloat3* a, const TerraFloat3* b ) {
    TerraFloat3 delta = terra_subf3 ( b, a );
    return terra_dotf3 ( &delta, &delta );
}

inline TerraFloat3 terra_normf3 ( const TerraFloat3* vec ) {
    // Substitute with inverse len
    float len = terra_lenf3 ( vec );
    return terra_f3_set (
               vec->x / len,
               vec->y / len,
               vec->z / len );
}

inline TerraFloat3 terra_transformf3 ( const TerraFloat4x4* transform, const TerraFloat3* vec ) {
    return terra_f3_set ( terra_dotf3 ( ( TerraFloat3* ) &transform->rows[0], vec ),
                          terra_dotf3 ( ( TerraFloat3* ) &transform->rows[1], vec ),
                          terra_dotf3 ( ( TerraFloat3* ) &transform->rows[2], vec ) );
}

inline float terra_lerp ( float a, float b, float t ) {
    return a + ( b - a ) * t;
}

inline TerraFloat3 terra_lerpf3 ( const TerraFloat3* a, const TerraFloat3* b, float t ) {
    float x = terra_lerp ( a->x, b->x, t );
    float y = terra_lerp ( a->y, b->y, t );
    float z = terra_lerp ( a->z, b->z, t );
    return terra_f3_set ( x, y, z );
}


inline float terra_absf ( float a ) {
    return a >= 0 ? a : -a;
}

inline float terra_radians ( float degrees ) {
    return degrees * TERRA_PI / 180.f;
}

/*
    Returns the square root of the next power of two squared from v
    of v if v is a power of 2 squared
*/
inline uint64_t terra_next_pow2sq ( uint64_t v ) {
    uint64_t expected = 1;

    while ( expected * expected < v ) {
        ++expected;
    }

    return expected;
}

inline float terra_radical_inverse ( uint64_t base, uint64_t a ) {
    float inv_base = 1.f / base;
    uint64_t seq = 0;
    float denom = 1;

    while ( a ) {
        uint64_t next = a / base;
        uint64_t digit = a - next * base;
        seq = seq * base + digit;
        denom *= inv_base;
        a = next;
    }

    return TERRA_MIN ( seq * denom, 1.f - TERRA_EPS );
}

inline bool terra_is_integer ( float v ) {
    return ( roundf ( v ) - v ) < TERRA_EPS;
}