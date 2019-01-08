// Header
#include <Terra.h>

// Terra
#include "TerraPrivate.h"

// libc
#include <string.h>

// SSE2
#include <xmmintrin.h>

//--------------------------------------------------------------------------------------------------
// (BVH) Spatial acceleration structures for single ray scene traversal.
// Use: #define terra_acceleration_stackless to enable scene hierachy traversal without using heap memory
// The available accelerations structures are listed and described below:
#define terra_acceleration_bvh                   1
#define terra_acceleration_multi_branching_bvh   0 // [Wald 2008]
#define terra_acceleration_multi_bvh_ray_stream  0 // [Tsakok 2009]
#define terra_acceleration_qbvh                  0 // [Dammertz and Keller 2008]
//
// # terra_acceleration_bvh
//
// # terra_acceleration_multi_branching_bvh
//   The spatial acceleration structure uses a multi-branching BVH as described in [Wald 2008] with
//   stackless single ray traversal [Hapala et al 2011].
//   It has better performance than naive BVHs and also works well for secondary rays, but it can perform
//   worse than packet streaming on primary ray due to their higher-coherence.
//   Given that we only have an unidirectional path tracer, it seems reasonable to implement
//   this to hope for better performance on more light paths.
//
//   N = platform SIMD width (16)
//   The binary-BVH is built with the typical surface area heuristic (SAH) for splitting, but since intersecting N
//   triangles costs as much as 1 (branching), the leaf cost should account for it with a costant cost:
//
//       C_leaf = K_intersection * ceil(n_triangles_in_leaf / N)
//
//       where K_intersection is the constant cost that models a SIMD triangle test.
//
//   Rather than collapsing the lower leafs into nodes, the multi-node branching splitting is
//   handled with the greedy top-down recursive strategy proposed in the same paper [Wald 2008].
//   Starting with an empty list of potential nodes, they are split until they equal the branching
//   factor, at which point the procedure is called recursively.
//   A binned SAH heuristic with the constants described above is used to decide if a node is to be split.
//
//   High-level BVH single ray traversal:
//       - Get the  ray
//       - Inner node: Check against 16 children's AABB while traversing queued nodes ray depth first
//       - Leaf node: Test against 16 triangles and pick ray depth
//--------------------------------------------------------------------------------------------------
#if 0
#define SIMD_BRANCHING 16

typedef struct {
    __m128 min_x, max_x;
    __m128 min_y, max_y;
    __m128 min_z, max_z;
} MultiBVHNode;

typedef struct {

} MultiBVHLeaf;

#endif

typedef struct {
    int a;
} MultiBranchingBVH;

HTerraSpatialAcceleration  terra_spatial_acceleration_create ( HTerraScene scene ) {
    MultiBranchingBVH* bvh = terra_malloc ( sizeof ( MultiBranchingBVH ) );
    memset ( bvh, 0, sizeof ( MultiBranchingBVH ) );

    return bvh;
}

void terra_spatial_acceleration_destroy ( HTerraSpatialAcceleration accel ) {
    terra_free ( accel );
}

int  terra_spatial_acceleration_query ( HTerraSpatialAcceleration accel, const TerraRay* ray, TerraRayIntersectionResult* result ) {

}
