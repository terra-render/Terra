//--------------------------------------------------------------------------------------------------
// Math inline implementation
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

inline TerraFloat3 terra_f3_setv ( const float* xyz ) {
    TerraFloat3 ret;
    ret.x = xyz[0];
    ret.y = xyz[1];
    ret.z = xyz[2];
    return ret;
}

inline TerraFloat4 terra_f4_set ( float x, float y, float z, float w ) {
    TerraFloat4 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;
    return ret;
}

inline TerraInt4 terra_i4_set ( int x, int y, int z, int w ) {
    TerraInt4 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;
    return ret;
}

inline bool terra_equalf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
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

inline float terra_sqlenf3 ( const TerraFloat3* vec ) {
    return
        vec->x * vec->x +
        vec->y * vec->y +
        vec->z * vec->z;
}

inline float terra_distf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
    TerraFloat3 ba = terra_subf3 ( a, b );
    return terra_lenf3 ( &ba );
}

inline float terra_sqdistf3 ( const TerraFloat3* a, const TerraFloat3* b ) {
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

inline int terra_max_coefff3 ( const TerraFloat3* vec ) {
    if ( vec->x > vec->y ) {
        if ( vec->x > vec->z ) {
            return 0;
        } else {
            return 2;
        }
    } else {
        if ( vec->y > vec->z ) {
            return 1;
        } else {
            return 2;
        }
    }
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

inline int terra_signf ( float v ) {
    return v == 0.f ? 0 :
           ( ( v > 0.f ) ? 1 : -1 );
}

inline uint32_t terra_signf_mask ( float v ) {
    const uint32_t sign_bit_mask = 0x80000000;
    return * ( ( uint32_t* ) &v ) & sign_bit_mask;
}

inline float terra_maxf3 ( const TerraFloat3* vec ) {
    return fmaxf ( vec->x, fmaxf ( vec->y, vec->z ) );
}

inline float terra_min3 ( const TerraFloat3* vec ) {
    return fminf ( vec->x, fminf ( vec->y, vec->z ) );
}

inline TerraFloat3 terra_absf3 ( const TerraFloat3* vec ) {
    return terra_f3_set ( fabs ( vec->x ), fabs ( vec->y ), fabs ( vec->z ) );
}

inline float terra_xorf ( float _lhs, float _rhs ) {
    uint32_t* lhs = ( uint32_t* ) &_lhs;
    uint32_t* rhs = ( uint32_t* ) &_rhs;
    uint32_t res = ( *lhs ) ^ ( *rhs );
    return * ( float* ) &res;
}

inline void terra_swap_xorf ( float* _a, float* _b ) {
    uint32_t* a = ( uint32_t* ) _a;
    uint32_t* b = ( uint32_t* ) _b;
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
}

inline void terra_swap_xori ( int* a, int* b ) {
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
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

inline TerraFloat4x4 terra_f4x4_basis ( const TerraFloat3* normal ) {
    TerraFloat3 tangent;
    TerraFloat3 bitangent;
    TerraFloat4x4 xform;

    // Hughes-Möller to get vector perpendicular to normal
    // http://www.pbr-book.org/3ed-2018/Geometry_and_Transformations/Vectors.html#CoordinateSystemfromaVector
    if ( fabs ( normal->x ) > fabs ( normal->y ) ) {
        tangent = terra_f3_set ( normal->z, 0.f, -normal->x );
        tangent = terra_mulf3 ( &tangent, sqrtf ( normal->x * normal->x + normal->z * normal->z ) );
    } else {
        tangent = terra_f3_set ( 0.f, -normal->z, normal->y );
        tangent = terra_mulf3 ( &tangent, sqrtf ( normal->y * normal->y + normal->z * normal->z ) );
    }

    bitangent = terra_crossf3 ( normal, &tangent );
    xform.rows[0] = terra_f4_set ( tangent.x, normal->x, bitangent.x, 0.f );
    xform.rows[1] = terra_f4_set ( tangent.y, normal->y, bitangent.y, 0.f );
    xform.rows[2] = terra_f4_set ( tangent.z, normal->z, bitangent.z, 0.f );
    xform.rows[3] = terra_f4_set ( 0.f, 0.f, 0.f, 1.f );
    return xform;
}

inline TerraFloat3 terra_f4x4_get_normal ( const TerraFloat4x4* basis ) {
    return terra_f3_set ( basis->rows[0].y, basis->rows[1].y, basis->rows[2].y );
}

inline TerraFloat3 terra_f4x4_get_tangent ( const TerraFloat4x4* basis ) {
    return terra_f3_set ( basis->rows[0].x, basis->rows[1].x, basis->rows[2].x );
}

inline TerraFloat3 terra_f4x4_get_bitangent ( const TerraFloat4x4* basis ) {
    return terra_f3_set ( basis->rows[0].z, basis->rows[1].z, basis->rows[2].z );
}

inline float terra_clamp ( float value, float min, float max ) {
    return value < min ? min : value > max ? max : value;
}

inline TerraFloat3 terra_clampf3 ( const TerraFloat3* f3, const TerraFloat3* min, const TerraFloat3* max ) {
    TerraFloat3 res;
    res.x = f3->x > min->x ? f3->x : min->x;
    res.y = f3->y > min->y ? f3->y : min->y;
    res.z = f3->z > min->z ? f3->z : min->z;
    res.x = res.x < max->x ? res.x : max->x;
    res.y = res.y < max->y ? res.y : max->y;
    res.z = res.z < max->z ? res.z : max->z;
    return res;
}

inline float terra_sqr ( float value ) {
    return value * value;
}