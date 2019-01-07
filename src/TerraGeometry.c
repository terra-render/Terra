// Header
#include "TerraPrivate.h"

// Terra
#include <TerraProfile.h>
#include <TerraPresets.h>

// Returns the point along the ray at the specified depth
TerraFloat3 terra_ray_pos ( const TerraRay* ray, float depth ) {
    const TerraFloat3 d = terra_mulf3 ( &ray->direction, depth );
    return terra_addf3 ( &ray->origin, &d );
}

// Initializes any state associated with the ray (technically depending on the features enabled todo)
void terra_ray_state_init ( const TerraRay* ray, TerraRayState* state ) {
    terra_ray_triangle_intersection_init ( ray, state );
    terra_ray_box_intersection_init ( ray, state );
}

//--------------------------------------------------------------------------------------------------
// The Ray/Primitive intersections tests available are listed below. Note that only one should be enabled
// for each type of primitive.
// Ray/Triangle
#define ray_triangle_intersection_moller_trumbore 1 // Naive Moller-Trumbore test
#define ray_triangle_intersection_wald2013 0        // Faster (vertex/edge) watertight intersection algorithm
#define ray_triangle_intersection_wald2013_simd 0   // Simd version of the same algorithm

// Ray/Box
#define ray_box_branchless 1
#define ray_box_branchless_simd 0
#define ray_box_wald2013 0
#define ray_box_wald2013_simd 0

//--------------------------------------------------------------------------------------------------
#if ray_triangle_intersection_moller_trumbore
void terra_ray_triangle_intersection_init ( const TerraRay* ray, TerraRayState* state ) {
    TERRA_UNUSED ( ray );
    TERRA_UNUSED ( state );
}

int terra_ray_triangle_intersection_query ( const TerraRayIntersectionQuery* query, TerraRayIntersectionResult* result ) {
    int ret = 0;

    TerraClockTime profile_time_begin = TERRA_CLOCK();

    const TerraTriangle* tri = query->primitive.triangle;
    TerraFloat3 e1, e2, h, s, q;
    float a, f, u, v, t;
    e1 = terra_subf3 ( &tri->b, &tri->a );
    e2 = terra_subf3 ( &tri->c, &tri->a );
    h = terra_crossf3 ( &query->ray->direction, &e2 );
    a = terra_dotf3 ( &e1, &h );

    if ( a > -terra_Epsilon && a < terra_Epsilon ) {
        goto exit;
    }

    f = 1 / a;
    s = terra_subf3 ( &query->ray->origin, &tri->a );
    u = f * ( terra_dotf3 ( &s, &h ) );

    if ( u < 0.0 || u > 1.0 ) {
        goto exit;
    }

    q = terra_crossf3 ( &s, &e1 );
    v = f * terra_dotf3 ( &query->ray->direction, &q );

    if ( v < 0.0 || u + v > 1.0 ) {
        goto exit;
    }

    t = f * terra_dotf3 ( &e2, &q );

    if ( t > terra_Epsilon ) {
        TerraFloat3 offset = terra_mulf3 ( &query->ray->direction, t );
        result->point = terra_addf3 ( &offset, &query->ray->origin );
        result->ray_depth = t;

        ret = 1;
        goto exit;
    }

exit:
    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, ( TERRA_CLOCK() - profile_time_begin ) );
    return ret;
}
#endif

// The init is shared between the two versions
#if ray_triangle_intersection_wald2013 || ray_triangle_intersection_wald2013_simd

// Precomputing ray transformation to origin and Z-pointing upwards for the intersection test
// The transformation is such that the ray will have origin in (0, 0, 0) and direction in z (0, 0, 1)
// The affine transformation is done through M = translation * shear * scale, which takes less operations
// and results in smaller rounding error.
// The algorithm produces watertight results as the floating point model guarantees the ordering
// of real numbers after rounding preserving the correctness ((M*B).x * (M*A).y >= (M*B).y * (M*A).x) of the edge test.
void terra_ray_triangle_intersection_init ( const TerraRay* ray, TerraRayState* state ) {
    int ret = 0;
    TerraClockTime profile_time_begin = TERRA_CLOCK();

    // First, we need to guarantee that ray.dir has the largest absolute value in by rotating the indices
    // preserving winding direction. Also, if z is negative we need to flip the winding direction which
    // amounts to swapping the x/y axis
    float* dir = ( float* ) &ray->direction;
    const TerraFloat3 ray_dir_abs = terra_absf3 ( ( TerraFloat3* ) dir );
    int iz = terra_max_coefff3 ( &ray_dir_abs );
    int ix = iz + 1;

    if ( ix == 3 ) {
        ix = 0;
    }

    int iy = ix + 1;

    if ( iy == 3 ) {
        iy = 0;
    }

    if ( dir[iz] < 0.f ) {
        int tmp = ix;
        ix = iy;
        iy = tmp;
        //terra_swap_xori ( &ix, &iy );
    }

    // Shear factors for the triangle coordinates be flat on the xy plane (?)
    float scalez = 1.f / dir[iz];
    float shearx = dir[ix] * scalez;
    float sheary = dir[iy] * scalez;

    // Storing scale factors in ray state
    state->ray_transform_f4 = terra_f4_set ( shearx, sheary, scalez, 0.f );
    state->ray_transform_i4 = terra_i4_set ( ix, iy, iz, 0 );

    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, ( TERRA_CLOCK() - profile_time_begin ) );
    return ret;
}

#endif

#if ray_triangle_intersection_wald2013

// Performs the ray/edge test in Pluecker coordinates after reducing the problem to 2D with the
// precomputed ray transformation coefficients. The details on the original version of the algorithm
// are in [Wald 2004][Bentin 2006].
// Fundamentally, it exploits the property of the dot-product, for a ray R and E one of the edges
// of the triangle (A, B, C): ray = [dir, dir x origin] edge = [A - B, A x B]
// The dot product: ray o edge = dir o (A-B) + (dir x origin) o (A x B)
// return: = 0 if the two lines intersect (fallback to double precision)
//         > 0 if the two lines two lines pass each other clockwise
//         < 0 if the two lines two lines pass each other counter-clockwise
// The intersection test checks that all the ray/edge tests return the same sign, backface culling
// is not performed here ( both >0 and <0 will work regardless of the ray direction)
//
// The function returns 1 if the ray intersects the triangle, 0 otherwise
// Note: it assumes that RayState::intersection_transform has been computed in the above init() function)
int terra_ray_triangle_intersection_query ( const TerraRayIntersectionQuery* q, TerraRayIntersectionResult* result ) {
    int ret = 0;
    TerraClockTime profile_time_begin = TERRA_CLOCK();

    // Getting those nicely computed indices back (I'm not sure it's worth the extra memory..)
    int ix = q->state->ray_transform_i4.x;
    int iy = q->state->ray_transform_i4.y;
    int iz = q->state->ray_transform_i4.z;

    // Also getting the transformation elements
    float shearx = q->state->ray_transform_f4.x;
    float sheary = q->state->ray_transform_f4.y;
    float scalez = q->state->ray_transform_f4.z;

    // Moving triangle frame to ray origin
    const TerraFloat3 _A = terra_subf3 ( &q->primitive.triangle->a, &q->ray->origin );
    const TerraFloat3 _B = terra_subf3 ( &q->primitive.triangle->b, &q->ray->origin );
    const TerraFloat3 _C = terra_subf3 ( &q->primitive.triangle->c, &q->ray->origin );
    const float* A = ( float* ) &_A; // sigh...
    const float* B = ( float* ) &_B;
    const float* C = ( float* ) &_C;

    // Finish transformation by shear and scale
    // | 1 0 -shearx
    // | 0 1 -sheary
    // | 0 0 scalez
    const float Ax = A[ix] - shearx * A[iz];
    const float Ay = A[iy] - sheary * A[iz];
    const float Bx = B[ix] - shearx * B[iz];
    const float By = B[iy] - sheary * B[iz];
    const float Cx = C[ix] - shearx * C[iz];
    const float Cy = C[iy] - sheary * C[iz];

    // Now, since the direction of the ray is (0, 0, 1), calculating the barycentric coordinates
    // and performing the intersection test results in simpler formulas. A, B, C are in the coordinate
    // system with the origin at the ray's one and scaled.
    // U = dot(ray.dir, cross(C, B))
    // V = dot(ray.dir, cross(A, C))
    // W = dot(ray.dir, cross(B, A))
    // expanding with ray.dir = (0, 0, 1) yields
    float U = Cx * By - Cy * Bx;
    float V = Ax * Cy - Ay * Cx;
    float W = Bx * Ay - By * Ax;

    // Retry the edge test with double-precision if any barycentric coordinate is too small for float32
    if ( U == 0.f || V == 0.f || W == 0.f ) {
        U = ( float ) ( ( double ) Cx * ( double ) By - ( double ) Cy * ( double ) Bx );
        V = ( float ) ( ( double ) Ax * ( double ) Cy - ( double ) Ay * ( double ) Cx );
        W = ( float ) ( ( double ) Bx * ( double ) Ay - ( double ) By * ( double ) Ax );
    }

    // Is the intersection point outside the triangle ?
    // (any negative barycentric coordinate) and (any positive barycentric coordinate)
    // no back-face culling, either sign is ok
    uint32_t sign_mask = terra_signf_mask ( U );

    if ( sign_mask != terra_signf_mask ( V ) ) {
        goto exit;
    }

    if ( sign_mask != terra_signf_mask ( W ) ) {
        goto exit;
    }

    // If the determinant of the system is 0, the matrix cannot be inverted as
    // the ray is coplanar with the triangle.
    float det = U + V + W;

    if ( det == 0.f ) {
        goto exit;
    }

    // Finally, calculating the scaled hit distance, leaving the normalization of the coordinates
    // (division by determinant) as the last operation to be performed.
    // The remaining tests in the algorithm are:
    //   1. The ray is behind a previously hit-ray
    //   2. Back-face culling checking the sign of the ray depth (< 0 -> miss)
    //   3. The ray is behind the origin
    // 1. is performed by the caller, 2. is only for back-face culling, which is not done
    const float Az = scalez * A[iz];
    const float Bz = scalez * B[iz];
    const float Cz = scalez * C[iz];
    const float ray_depth = U * Az + V * Bz + W * Cz;

    if ( terra_xorf ( ray_depth, TERRA_AS ( sign_mask, float ) ) < 0.f ) {
        goto exit;
    }

    const float inv_det  = 1.f / det;
    result->u = U * inv_det;
    result->v = V * inv_det;
    result->w = W * inv_det;
    result->ray_depth = ray_depth * inv_det;
    result->point = terra_ray_pos ( q->ray, result->ray_depth );
    ret = 1;

exit:
    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, ( TERRA_CLOCK() - profile_time_begin ) );

    return ret;
}
#endif

#if ray_triangle_intersection_wald2013_simd

int terra_ray_triangle_intersection_query ( const TerraRayIntersectionQuery* q, TerraRayIntersectionResult* result ) {

}

#endif

//--------------------------------------------------------------------------------------------------
// Terra Ray/box intersection tests
//--------------------------------------------------------------------------------------------------
void terra_ray_box_intersection_init ( const TerraRay* ray, TerraRayState* state ) {

}

int terra_ray_box_intersection_query ( const TerraRayIntersectionQuery* q, TerraRayIntersectionResult* result ) {

}