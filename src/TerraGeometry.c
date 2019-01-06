// Header
#include "TerraPrivate.h"

// Terra
#include <TerraProfile.h>
#include <TerraPresets.h>

/*
    Ray/Triangle intersection tests available:
     - ray_triangle_moller_trumbore (naive)
     - ray_triangle_wald_2013 (watertight, more numerically stable) from [Wald 2013] A fast watertight ray/triangle intersection algorithm
     - ray_triangle_wald_2013_simd same as above, for a single ray/triangle intersection
*/
#define ray_triangle_intersection_moller_trumbore 0
#define ray_triangle_intersection_wald2013 1
#define ray_triangle_intersection_wald2013_simd 0

#if ray_triangle_intersection_moller_trumbore
int terra_geom_ray_triangle_intersection ( const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out, float* t_out ) {
    int ret = 0;

    TerraClockTime profile_time_begin = TERRA_CLOCK();

    const TerraTriangle* tri = triangle;
    TerraFloat3 e1, e2, h, s, q;
    float a, f, u, v, t;
    e1 = terra_subf3 ( &tri->b, &tri->a );
    e2 = terra_subf3 ( &tri->c, &tri->a );
    h = terra_crossf3 ( &ray->direction, &e2 );
    a = terra_dotf3 ( &e1, &h );

    if ( a > -terra_Epsilon && a < terra_Epsilon ) {
        goto exit;
    }

    f = 1 / a;
    s = terra_subf3 ( &ray->origin, &tri->a );
    u = f * ( terra_dotf3 ( &s, &h ) );

    if ( u < 0.0 || u > 1.0 ) {
        goto exit;
    }

    q = terra_crossf3 ( &s, &e1 );
    v = f * terra_dotf3 ( &ray->direction, &q );

    if ( v < 0.0 || u + v > 1.0 ) {
        goto exit;
    }

    t = f * terra_dotf3 ( &e2, &q );

    if ( t > terra_Epsilon ) {
        TerraFloat3 offset = terra_mulf3 ( &ray->direction, t );
        *point_out = terra_addf3 ( &offset, &ray->origin );

        if ( t_out != NULL ) {
            *t_out = t;
        }

        ret = 1;
        goto exit;
    }

exit:
    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, ( TERRA_CLOCK() - profile_time_begin ) );
    return ret;
}
#endif

#if ray_triangle_intersection_wald2013 || ray_triangle_intersection_wald2013_simd

// Precomputing ray transformation to origin and Z-pointing upwards for the intersection test
// The transformation is such that the ray will have origin in (0, 0, 0) and direction in z (0, 0, 1)
// The affine transformation is a translation * shear * scale
int terra_geom_ray_triangle_intersection_init ( const TerraRay* ray, TerraRayState* state ) {
    int ret = 0;
    TerraClockTime profile_time_begin = TERRA_CLOCK();

    // First, we need to guarantee that ray.dir has the largest absolute value in z and invert the winding
    // direction of the triangles when z is negative < 0.
    int max_dir = ray->direction.z > ray->direction.y;

    // Rotating the indices
    int iz = 0;
    int ix = ( iz == 2 ) ? 0 : iz + 1;
    int iy = ( ix == 3 ) ? 0 : ix + 1;

    // Shear factors for the triangle coordinates be flat on the xy plane (?)
    float scalez = 1.f / ray->direction.z;
    float shearx = ray->direction.x * scalez;
    float sheary = ray->direction.y * scalez;

    // Storing scale factors in ray state
    state->ray_transform_f4 = terra_f4_set ( shearx, sheary, scalez, 0.f );
    state->ray_transform_i4 = terra_i4_set ( ix, iy, iz, 0 );

    if ( ray->direction.z < 0.f ) {
        terra_swap_xori ( &ix, &iy );
    }

    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, ( TERRA_CLOCK() - profile_time_begin ) );
    return ret;
}

// Returns 0 if the ray intersects the triangle
// (assuming RayState::intersection_transform has been computed in the above init() function)
int terra_geom_ray_triangle_intersection_query ( const TerraRayIntersectionQuery* q, TerraRayIntersectionResult* result ) {
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
    float U = Cx * By - Cy * Bx;
    float V = Ax * Cy - Ay * Cx;
    float W = Bx * Ay - By * Ax;

    // Edge test (redoing it for double if delta if difference is too small for float32
    if ( U == 0.f || V == 0.f || W == 0.f ) {
        U = ( float ) ( ( double ) Cx * ( double ) By - ( double ) Cy * ( double ) Bx );
        V = ( float ) ( ( double ) Ax * ( double ) Cy - ( double ) Ay * ( double ) Cx );
        W = ( float ) ( ( double ) Bx * ( double ) Ay - ( double ) By * ( double ) Ax );
    }

    // Is the intersection point outside the triangle ?
    // (any negative barycentric coordinate) and (any positive barycentric coordinate)
    if ( ( U < 0.f || V < 0.f || W < 0.f ) && ( U > 0.f || V > 0.f || W > 0.f ) ) {
        goto exit;
    }

    // Determinant of the system of equations above (why, do math..? todo) used to check if
    // the ray is coplanar with the triangle3
    float det = U + V + W;

    if ( det == 0.f ) {
        goto exit;
    }

    // Finally, calculating the scaled hit distance, leaving the normalization of the coordinates
    // (division by determinant) as the last operation to be performed.
    // The last failure cases are if the hit is behind the triangle (z < 0) or behind an already-found hit
    // (the latter of the tests is done in the caller routine)
    const float Az = scalez * A[iz];
    const float Bz = scalez * B[iz];
    const float Cz = scalez * C[iz];
    const float ray_depth = U * Az + V * Bz + W * Cz;

    if ( ray_depth < 0.f ) {
        goto exit;
    }

    const float inv_det  = 1.f / det;
    result->u = U * inv_det;
    result->v = V * inv_det;
    result->w = W * inv_det;
    result->ray_depth = ray_depth * inv_det;

exit:
    TERRA_PROFILE_ADD_SAMPLE ( time, TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, ( TERRA_CLOCK() - profile_time_begin ) );

    return ret;
}
#endif

int terra_geom_ray_box_intersection_init ( const TerraRay* ray, TerraRayState* state ) {

}

int terra_geom_ray_box_intersection_query ( const TerraRayIntersectionQuery* q, TerraRayIntersectionResult* result ) {

}