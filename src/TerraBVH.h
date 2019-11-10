#ifndef _TERRA_BVH_H_
#define _TERRA_BVH_H_

// Terra
#include <Terra.h>
#include <TerraMath.h>
#include "TerraPrivate.h"

// libc
#include <stdint.h>

// Node of the BVH tree. Fits in a 64 byte cache line.
typedef struct {
    TerraAABB aabb[2]; // Left and right AABBs, one for each sub-volume
    int32_t index[2];  // Index of the BVH node representing each sub-volume, or index of the model/triangle if sub-volume is leaf
    int32_t type[2];   // -1 if sub-volume is not leaf, 1 if it's leaf and contains a single triangle
} TerraBVHNode;

typedef struct {
    TerraBVHNode* nodes;
    int           nodes_count;
} TerraBVH;

//--------------------------------------------------------------------------------------------------
// Terra BVH Internal routines
//--------------------------------------------------------------------------------------------------
void        terra_bvh_create ( TerraBVH* bvh, const TerraObject* objects, int objects_count );
void        terra_bvh_destroy ( TerraBVH* bvh );
bool        terra_bvh_traverse ( TerraBVH* bvh, const TerraObject* objects, const TerraRay* ray, const TerraRayState* ray_state,
                                 TerraFloat3* point_out, TerraPrimitiveRef* primitive_out );

#endif // _TERRA_BVH_H_