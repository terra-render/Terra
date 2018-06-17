// Header
#include "TerraKDTree.h"

// libc
#include <string.h>

typedef struct TerraKDSplit {
    float offset;
    uint32_t left_count;
    uint32_t right_count : 31;
    uint32_t type : 1;
    uint16_t t0c;
    uint16_t t1c;
} TerraKDSplit;

#define terra__comp(float3, axis) *((float*)&float3 + axis) // saving space
#define TERRA_KDTREE_INTERSECTION_COST 1.5f // 0.32f
#define TERRA_KDTREE_TRAVERSAL_COST 0.8f

// Sorts the splits in ascending order
int terra_splitlist_ascending_cmpfun ( const void* a, const void* b ) {
    float af = ( ( TerraKDSplit* ) a )->offset;
    float bf = ( ( TerraKDSplit* ) b )->offset;
    return af == bf ? 0 : ( af < bf ) ? -1 : 1;
}

// Nodes are added in as a kd-tree always has either no children or both
// and reduces nodes size
int terra_kdtree_add_node_pair ( TerraKDTree* tree ) {
    if ( tree->nodes_count + 2 > tree->nodes_capacity ) {
        TerraKDNode* old_buffer = tree->nodes;
        int new_capacity = terra_maxi ( 64, tree->nodes_capacity * tree->nodes_capacity );
        tree->nodes = terra_malloc ( sizeof ( TerraKDNode ) * new_capacity );
        memcpy ( tree->nodes, old_buffer, sizeof ( TerraKDNode ) * tree->nodes_count );
        tree->nodes_capacity = new_capacity;
        terra_free ( old_buffer );
    }

    int ret = tree->nodes_count;
    tree->nodes_count += 2;
    return ret;
}

void terra_kdtree_destroy_object_buffer ( TerraKDObjectBuffer* buffer ) {
    if ( buffer == NULL || buffer->objects == NULL ) {
        return;
    }

    terra_free ( buffer->objects );
    buffer->objects = NULL;
    buffer->objects_count = 0;
}

void terra_kdtree_init_object_buffer ( TerraKDObjectBuffer* buffer, int num_objects ) {
    terra_kdtree_destroy_object_buffer ( buffer );
    buffer->objects = terra_malloc ( sizeof ( TerraKDObjectRef ) * num_objects );
    buffer->objects_count = num_objects;
}

int terra_kdtree_add_object_buffer ( TerraKDTree* tree ) {
    if ( tree->object_buffers_count + 1 > tree->object_buffers_capacity ) {
        TerraKDObjectBuffer* old_buffer = tree->object_buffers;
        int new_capacity = terra_maxi ( 64, tree->object_buffers_capacity * tree->object_buffers_capacity );
        tree->object_buffers = terra_malloc ( sizeof ( TerraKDObjectBuffer ) * new_capacity );
        memcpy ( tree->object_buffers, old_buffer, sizeof ( TerraKDObjectBuffer ) * tree->object_buffers_count );
        tree->object_buffers_capacity = new_capacity;
        terra_free ( old_buffer );
    }

    int ret = tree->object_buffers_count;
    tree->object_buffers[ret].objects = NULL;
    tree->object_buffers[ret].objects_count = 0;
    tree->object_buffers_count += 1;
    return ret;
}

bool terra_aabb_overlap ( const TerraAABB* left, const TerraAABB* right ) {
    return ( left->min.x <= right->max.x && left->max.x >= right->min.x ) &&
           ( left->min.y <= right->max.y && left->max.y >= right->min.y ) &&
           ( left->min.z <= right->max.z && left->max.z >= right->min.z );
}

void terra_kdtree_add_splitbuffer ( TerraKDSplit* splitbuffer, int* counter, float val, int type ) {
    for ( int i = 0; i < *counter; ++i ) {
        if ( val == splitbuffer[i].offset ) {
            if ( type == 0 ) {
                ++splitbuffer[i].t0c;
            } else {
                ++splitbuffer[i].t1c;
            }

            return;
        }
    }

    splitbuffer[*counter].offset = val;
    splitbuffer[*counter].type = type;
    splitbuffer[*counter].t0c = 1;
    splitbuffer[*counter].t1c = 1;
    ( *counter )++;
}

void terra_kdtree_create_rec ( TerraKDTree* tree, int node_idx, const TerraObject* objects, const TerraAABB* aabb, int depth, TerraKDSplit* splitbuffer, TerraAABB* aabb_cache ) {
    if ( depth <= 0 ) {
        return;
    }

    TerraKDNode* node = &tree->nodes[node_idx];
    TerraFloat3 extents;
    extents.x = aabb->max.x - aabb->min.x;
    extents.y = aabb->max.y - aabb->min.y;
    extents.z = aabb->max.z - aabb->min.z;
    // Cycling through the axis
    int axis = 2;

    if ( ( extents.x >= extents.y ) && ( extents.x >= extents.z ) ) {
        axis = 0;
    } else if ( ( extents.y >= extents.x ) && ( extents.y >= extents.z ) ) {
        axis = 1;
    }

    // Finding all split positions
    TerraKDObjectBuffer* buffer = &tree->object_buffers[node->leaf.objects];
    int splitlist_next = 0;

    for ( int i = 0; i < buffer->objects_count; ++i ) {
        TerraKDObjectRef ref = buffer->objects[i];
        TerraTriangle* triangle = &objects[ref.primitive.object_idx].triangles[ref.primitive.triangle_idx];
        aabb_cache[i].min = terra_f3_set1 ( FLT_MAX );
        aabb_cache[i].max = terra_f3_set1 ( -FLT_MAX );
        terra_aabb_fit_triangle ( &aabb_cache[i], triangle );
        TerraAABB* triangle_aabb = &aabb_cache[i];
        float p1 = terra__comp ( triangle_aabb->min, axis );
        terra_kdtree_add_splitbuffer ( splitbuffer, &splitlist_next, p1, 0 );
        float p2 = terra__comp ( triangle_aabb->max, axis );
        terra_kdtree_add_splitbuffer ( splitbuffer, &splitlist_next, p2, 1 );
    }

    qsort ( splitbuffer, splitlist_next, sizeof ( TerraKDSplit ), terra_splitlist_ascending_cmpfun );
    int right_counter = buffer->objects_count;
    int left_counter = 0;

    for ( int i = 0; i < splitlist_next; ++i ) {
        TerraKDSplit* split = &splitbuffer[i];

        if ( split->type == 0 ) {
            left_counter += split->t0c;
        }

        split->right_count = right_counter;
        split->left_count = left_counter;

        if ( split->type == 1 ) {
            right_counter -= split->t1c;
        }
    }

    // Estimating costs
    float SAV = 0.5f / ( extents.x * extents.z + extents.x * extents.y + extents.z * extents.y );
    float cost_of_not_splitting = TERRA_KDTREE_INTERSECTION_COST * ( float ) buffer->objects_count;
    float lowest_cost = FLT_MAX;
    float best_split = 0.f;
    int left_count = -1;
    int right_count = -1;

    for ( int i = 0; i < splitlist_next; ++i ) {
        TerraKDSplit* split = &splitbuffer[i];
        // Calculating extents
        TerraAABB left = *aabb, right = *aabb;
        terra__comp ( left.max, axis ) = split->offset;
        terra__comp ( right.min, axis ) = split->offset;
        TerraFloat3 le;
        le.x = left.max.x - left.min.x;
        le.y = left.max.y - left.min.y;
        le.z = left.max.z - left.min.z;
        TerraFloat3 re;
        re.x = right.max.x - right.min.x;
        re.y = right.max.y - right.min.y;
        re.z = right.max.z - right.min.z;
        float SA_left = 2 * ( le.x * le.z + le.x * le.y + le.z * le.y );
        float SA_right = 2 * ( re.x * re.z + re.x * re.y + re.z * re.y );
        float split_cost = TERRA_KDTREE_TRAVERSAL_COST + TERRA_KDTREE_INTERSECTION_COST * ( SA_left * SAV * split->left_count + SA_right * SAV * split->right_count );

        if ( split_cost < lowest_cost ) {
            lowest_cost = split_cost;
            best_split = split->offset;
            left_count = split->left_count;
            right_count = split->right_count;
        }
    }

    // Splitting is not worth
    if ( lowest_cost > cost_of_not_splitting ) {
        return;
    }

    // Saving the temporary buffer used by left
    int old_buffer = node->leaf.objects;
    // Time to split!
    int children = ( uint32_t ) terra_kdtree_add_node_pair ( tree );
    node = &tree->nodes[node_idx];
    node->internal.children = children;
    node->internal.is_leaf = 0;
    node->internal.axis = axis;
    node->split = best_split;
    TerraKDNode* left_node = &tree->nodes[node->internal.children];
    TerraKDNode* right_node = &tree->nodes[node->internal.children + 1];
    left_node->leaf.objects = terra_kdtree_add_object_buffer ( tree );
    right_node->leaf.objects = terra_kdtree_add_object_buffer ( tree );
    left_node->internal.is_leaf = 1;
    right_node->internal.is_leaf = 1;
    buffer = &tree->object_buffers[old_buffer];
    TerraKDObjectBuffer* left_buffer = &tree->object_buffers[left_node->leaf.objects];
    TerraKDObjectBuffer* right_buffer = &tree->object_buffers[right_node->leaf.objects];
    terra_kdtree_init_object_buffer ( left_buffer, left_count );
    terra_kdtree_init_object_buffer ( right_buffer, right_count );
    int lbc = 0;
    int rbc = 0;
    TerraAABB left_aabb = *aabb, right_aabb = *aabb;
    terra__comp ( left_aabb.max, axis ) = best_split;
    terra__comp ( right_aabb.min, axis ) = best_split;
    float left_max = best_split;
    float right_min = best_split;
    float left_min = terra__comp ( left_aabb.min, axis );
    float right_max = terra__comp ( right_aabb.max, axis );

    for ( int i = 0; i < buffer->objects_count; ++i ) {
        TerraKDObjectRef ref = buffer->objects[i];
        // TerraTriangle* triangle = &scene->objects[ref.primitive.object_idx].triangles[ref.primitive.triangle_idx];
        TerraAABB triangle_aabb = aabb_cache[i];
        float triangle_min = terra__comp ( triangle_aabb.min, axis );
        float triangle_max = terra__comp ( triangle_aabb.max, axis );

        if ( ( triangle_min <= left_max ) && ( triangle_max >= left_min ) ) {
            left_buffer->objects[lbc++] = ref;
        }

        if ( ( triangle_min <= right_max ) && ( triangle_max >= right_min ) ) {
            right_buffer->objects[rbc++] = ref;
        }
    }

    assert ( lbc == left_buffer->objects_count );
    assert ( rbc == right_buffer->objects_count );
    terra_kdtree_destroy_object_buffer ( buffer );

    // Not splitting for less than 3..
    if ( left_count > 3 ) {
        terra_kdtree_create_rec ( tree, children, objects, &left_aabb, depth - 1, splitbuffer, aabb_cache );
    }

    if ( right_count > 3 ) {
        terra_kdtree_create_rec ( tree, children + 1, objects, &right_aabb, depth - 1, splitbuffer, aabb_cache );
    }
}

void terra_kdtree_create ( TerraKDTree* kdtree, const TerraObject* objects, int objects_count ) {
    int primitives_count = 0;

    for ( int j = 0; j < objects_count; ++j ) {
        primitives_count += objects[j].triangles_count;
    }

    kdtree->nodes = terra_malloc ( sizeof ( TerraKDNode ) );
    kdtree->nodes_count = 1;
    kdtree->nodes_capacity = 1;
    kdtree->object_buffers = NULL;
    kdtree->object_buffers_count = 0;
    kdtree->object_buffers_capacity = 0;
    kdtree->nodes[0].internal.is_leaf = 1;
    kdtree->nodes[0].leaf.objects = terra_kdtree_add_object_buffer ( kdtree );
    TerraKDObjectBuffer* buffer = &kdtree->object_buffers[kdtree->nodes[0].leaf.objects];
    terra_kdtree_init_object_buffer ( buffer, primitives_count );
    kdtree->scene_aabb.min = terra_f3_set1 ( FLT_MAX );
    kdtree->scene_aabb.max = terra_f3_set1 ( -FLT_MAX );
    // Copying all triangles to temporary buffer
    int pidx = 0;

    for ( int j = 0; j < objects_count; ++j ) {
        for ( size_t i = 0; i < objects[j].triangles_count; ++i, ++pidx ) {
            TerraTriangle* triangle = &objects[j].triangles[i];
            terra_aabb_fit_triangle ( &kdtree->scene_aabb, triangle );
            buffer->objects[pidx].primitive.object_idx = j;
            buffer->objects[pidx].primitive.triangle_idx = i;
            buffer->objects[pidx].triangle = *triangle;
        }
    }

    buffer->objects_count = pidx;
    TerraKDSplit* splitbuffer = terra_malloc ( sizeof ( TerraKDSplit ) * primitives_count * 2 );
    TerraAABB* aabb_cache = terra_malloc ( sizeof ( TerraAABB ) * primitives_count * 2 );
    terra_kdtree_create_rec ( kdtree, 0, objects, &kdtree->scene_aabb, 20, splitbuffer, aabb_cache );
    terra_free ( splitbuffer );
    terra_free ( aabb_cache );
}

//--------------------------------------------------------------------------------------------------
void terra_kdtree_destroy ( TerraKDTree* kdtree ) {
    terra_free ( kdtree->nodes );

    for ( int i = 0; i < kdtree->object_buffers_count; ++i ) {
        terra_kdtree_destroy_object_buffer ( &kdtree->object_buffers[i] );
    }

    terra_free ( kdtree->object_buffers );
}

//--------------------------------------------------------------------------------------------------
bool terra_kdtree_traverse ( TerraKDTree* kdtree, const TerraRay* ray, TerraFloat3* point_out, TerraPrimitiveRef* primitive_out ) {
    float a, b;
    float t;

    if ( !terra_ray_aabb_intersection ( ray, &kdtree->scene_aabb, &a, &b ) ) {
        return false;
    }

    typedef struct StackEntry {
        TerraKDNode* node;
        float t;
        TerraFloat3 pb;
        int prev;
    } StackEntry;
    StackEntry stack[64];
    TerraKDNode* far_child, *cur_node;
    cur_node = &kdtree->nodes[0];
    int enpt = 0;
    stack[enpt].t = a;

    if ( a >= 0.f ) {
        TerraFloat3 d = terra_mulf3 ( &ray->direction, a );
        stack[enpt].pb = terra_addf3 ( &ray->origin, &d );
    } else {
        stack[enpt].pb = ray->origin;
    }

    int expt = 1;
    stack[expt].t = b;
    TerraFloat3 d = terra_mulf3 ( &ray->direction, b );
    stack[expt].pb = terra_addf3 ( &ray->origin, &d );
    stack[expt].node = NULL;

    while ( cur_node != NULL ) {
        while ( !cur_node->internal.is_leaf ) {
            float splitval = cur_node->split;
            int na_lu[3] = { 1, 2, 0 };
            int pa_lu[3] = { 2, 0, 1 };
            int axis = cur_node->internal.axis;
            int next_axis = na_lu[axis];
            int prev_axis = pa_lu[axis];
            TerraKDNode* left_node = &kdtree->nodes[cur_node->internal.children];
            TerraKDNode* right_node = left_node + 1;

            if ( terra__comp ( stack[enpt].pb, axis ) <= splitval ) {
                if ( terra__comp ( stack[expt].pb, axis ) <= splitval ) {
                    cur_node = left_node;
                    continue;
                }

                if ( terra__comp ( stack[expt].pb, axis ) == splitval ) {
                    cur_node = right_node;
                    continue;
                }

                far_child = right_node;
                cur_node = left_node;
            } else {
                if ( splitval < terra__comp ( stack[expt].pb, axis ) ) {
                    cur_node = right_node;
                    continue;
                }

                far_child = left_node;
                cur_node = right_node;
            }

            t = ( splitval - terra__comp ( ray->origin, axis ) ) / terra__comp ( ray->direction, axis );
            int tmp = expt++;

            if ( expt == enpt ) {
                ++expt;
            }

            stack[expt].prev = tmp;
            stack[expt].t = t;
            stack[expt].node = far_child;
            terra__comp ( stack[expt].pb, axis ) = splitval;
            terra__comp ( stack[expt].pb, next_axis ) = terra__comp ( ray->origin, next_axis ) + t * terra__comp ( ray->direction, next_axis );
            terra__comp ( stack[expt].pb, prev_axis ) = terra__comp ( ray->origin, prev_axis ) + t * terra__comp ( ray->direction, prev_axis );
        }

        TerraKDObjectBuffer* buffer = &kdtree->object_buffers[cur_node->leaf.objects];
        bool hit = false;
        float closest_t = FLT_MAX;

        for ( int i = 0; i < buffer->objects_count; ++i ) {
            TerraKDObjectRef ref = buffer->objects[i];
            //            TerraTriangle* triangle = &scene->objects[ref.primitive.object_idx].triangles[ref.primitive.triangle_idx];
            float t;
            TerraFloat3 point;

            if ( terra_ray_triangle_intersection ( ray, &ref.triangle, &point, &t ) ) {
                if ( t >= stack[enpt].t && t <= stack[expt].t && t < closest_t ) {
                    closest_t = t;
                    *primitive_out = ref.primitive;
                    *point_out = point;
                    hit = true;
                }
            }
        }

        if ( hit ) {
            return true;
        }

        enpt = expt;
        cur_node = stack[expt].node;
        expt = stack[enpt].prev;
    }

    return false;
}
