//--------------------------------------------------------------------------------------------------
// Math implementation
//--------------------------------------------------------------------------------------------------
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

inline TerraFloat4 terra_f4 ( float x, float y, float z, float w ) {
    TerraFloat4 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;
    return ret;
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

inline float terra_distf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
    TerraFloat3 ba = terra_subf3 ( a, b );
    return terra_lenf3 ( &ba );
}

inline float terra_distance_squaredf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
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

inline float terra_maxf ( float a, float b ) {
    return a > b ? a : b;
}

inline float terra_minf ( float a, float b ) {
    return a < b ? a : b;
}

inline size_t terra_maxi ( size_t a, size_t b ) {
    return a > b ? a : b;
}

inline size_t terra_mini ( size_t a, size_t b ) {
    return a < b ? a : b;
}

inline float terra_maxf3 ( const TerraFloat3* vec ) {
    return fmaxf ( vec->x, fmaxf ( vec->y, vec->z ) );
}

inline float terra_min3 ( const TerraFloat3* vec ) {
    return fminf ( vec->x, fminf ( vec->y, vec->z ) );
}

inline void terra_swapf ( float* a, float* b ) {
    float t = *a;
    *a = *b;
    *b = t;
}

inline TerraFloat3 terra_transformf3 ( const TerraFloat4x4* transform, const TerraFloat3* vec ) {
    return terra_f3_set ( terra_dotf3 ( ( TerraFloat3* ) &transform->rows[0], vec ),
                          terra_dotf3 ( ( TerraFloat3* ) &transform->rows[1], vec ),
                          terra_dotf3 ( ( TerraFloat3* ) &transform->rows[2], vec ) );
}

inline bool terra_f3_is_zero ( const TerraFloat3* f3 ) {
    return f3->x == 0 && f3->y == 0 && f3->z == 0;
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

inline float terra_triangle_area ( const TerraFloat3* a, const TerraFloat3* b, const TerraFloat3* c ) {
    TerraFloat3 ab = terra_subf3 ( b, a );
    TerraFloat3 ac = terra_subf3 ( c, a );
    TerraFloat3 cross = terra_crossf3 ( &ab, &ac );
    return terra_lenf3 ( &cross ) / 2;
}
