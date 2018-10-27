#ifndef _TERRA_BVH_H_
#define _TERRA_BVH_H_

// Terra
#include <Terra.h>
#include <TerraMath.h>
#include "TerraPrivate.h"

// libc
#include <stdint.h>

// Node of the BVH tree. Fits in a 64 byte cache line.
// TODO why the [2] layout ?
typedef struct {
    TerraAABB aabb[2];
    int32_t index[2];
    int32_t type[2];
} TerraBVHNode;

typedef struct {
    TerraAABB aabb;
    unsigned int index;
    int type;
} TerraBVHVolume;

typedef struct {
    TerraBVHNode* nodes;
    int           nodes_count;
} TerraBVH;

//--------------------------------------------------------------------------------------------------
// Terra BVH Internal routines
//--------------------------------------------------------------------------------------------------
void        terra_bvh_create ( TerraBVH* bvh, const TerraObject* objects, int objects_count );
void        terra_bvh_destroy ( TerraBVH* bvh );
bool        terra_bvh_traverse ( TerraBVH* bvh, const TerraObject* objects, const TerraRay* ray,
                                 TerraFloat3* point_out, TerraPrimitiveRef* primitive_out );

#endif // _TERRA_BVH_H_