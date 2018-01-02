#ifndef _TERRA_KDTREE_H_
#define _TERRA_KDTREE_H_

// Terra
#include <Terra.h>

// KDTree
// O(N log(N)^2)
// Triangle is copied to avoid an additional redirection. 
typedef struct TerraKDObjectRef
{
    TerraTriangle    triangle;
    TerraPrimitiveRef primitive;
}TerraKDObjectRef;

typedef struct TerraKDObjectBuffer
{
    TerraKDObjectRef* objects;
    int             objects_count;
}TerraKDObjectBuffer;

typedef struct TerraKDNodeInternalData
{
    uint32_t is_leaf : 1;
    uint32_t axis : 2;
    uint32_t children;
}TerraKDNodeInternalData;

typedef struct TerraKDNodeLeafData
{
    uint32_t padding : 1;
    uint32_t objects : 31; // idx to ObjectBuffer
}TerraKDNodeLeafData;

typedef struct TerraKDNode
{
    float split;
    TerraKDNodeInternalData internal;
    TerraKDNodeLeafData leaf;
}TerraKDNode;

// Container for different allocators. Root is nodes[0]
typedef struct TerraKDTree
{
    TerraKDNode* nodes; 
    int nodes_count;
    int nodes_capacity;
    TerraKDObjectBuffer* object_buffers;
    int object_buffers_count;
    int object_buffers_capacity;
    TerraAABB scene_aabb;
}TerraKDTree;

//--------------------------------------------------------------------------------------------------
// Terra K-D Tree Internal routines
//--------------------------------------------------------------------------------------------------
void terra_kdtree_create(TerraKDTree* kdtree, const TerraScene* scene);
void terra_kdtree_destroy(TerraKDTree* kdtree);
bool terra_kdtree_traverse(TerraKDTree* kdtree, const TerraRay* ray, const TerraScene* scene, TerraFloat3* point_out, TerraPrimitiveRef* primitive_out);


#endif // _TERRA_KDTREE_H_