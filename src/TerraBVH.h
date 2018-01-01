#ifndef _TERRA_BVH_H_
#define _TERRA_BVH_H_

// terra
#include <Terra.h>

// BVH Public types
typedef struct TerraBVHNode
{
    TerraAABB aabb[2];
    int index[2];
    int type[2];
} TerraBVHNode;

typedef struct TerraBVHVolume
{
    TerraAABB aabb;
    unsigned int index;
    int type;
} TerraBVHVolume;

typedef struct TerraBVH
{
    TerraBVHNode*  nodes;
    int           nodes_count;
} TerraBVH;

//--------------------------------------------------------------------------------------------------
// Terra BVH Internal routines
//--------------------------------------------------------------------------------------------------
void       terra_bvh_create(TerraBVH* bvh, const TerraScene* scene);
void       terra_bvh_destroy(TerraBVH* bvh);
bool       terra_bvh_traverse(TerraBVH* bvh, const TerraRay* ray, const TerraScene* scene, TerraFloat3* point_out, TerraPrimitiveRef* primitive_out);
float      terra_aabb_surface_area(const TerraAABB* aabb);
TerraFloat3 terra_aabb_center(const TerraAABB* aabb);
int        terra_bvh_volume_compare_x(const void* left, const void* right);
int        terra_bvh_volume_compare_y(const void* left, const void* right);
int        terra_bvh_volume_compare_z(const void* left, const void* right);
int        terra_bvh_sah_split_volumes(TerraBVHVolume* volumes, int volumes_count, const TerraAABB* container);

#endif // _TERRA_BVH_H_