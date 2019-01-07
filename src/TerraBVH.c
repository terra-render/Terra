// TerraBVH
#include "TerraBVH.h"

// Terra
#include "TerraPrivate.h"

// libc
#include <assert.h>

#include <stdio.h> // todo: remove debug

static float       terra_aabb_surface_area ( const TerraAABB* aabb );
static TerraFloat3 terra_aabb_center ( const TerraAABB* aabb );
static int         terra_bvh_volume_compare_x ( const void* left, const void* right );
static int         terra_bvh_volume_compare_y ( const void* left, const void* right );
static int         terra_bvh_volume_compare_z ( const void* left, const void* right );
static int         terra_bvh_sah_split_volumes ( TerraBVHVolume* volumes, int volumes_count, const TerraAABB* container );

float terra_aabb_surface_area ( const TerraAABB* aabb ) {
    float w = aabb->max.x - aabb->min.x;
    float h = aabb->max.y - aabb->min.y;
    float d = aabb->max.z - aabb->min.z;
    return 2 * ( w * d + w * h + d * h );
}

TerraFloat3 terra_aabb_center ( const TerraAABB* aabb ) {
    TerraFloat3 center = terra_addf3 ( &aabb->min, &aabb->max );
    center = terra_divf3 ( &center, 2 );
    return center;
}

int terra_bvh_volume_compare_x ( const void* left, const void* right ) {
    const TerraAABB* left_aabb = & ( ( const TerraBVHVolume* ) left )->aabb;
    const TerraAABB* right_aabb = & ( ( const TerraBVHVolume* ) right )->aabb;

    if ( terra_aabb_center ( left_aabb ).x < terra_aabb_center ( right_aabb ).x ) {
        return true;
    }

    return false;
}

int terra_bvh_volume_compare_y ( const void* left, const void* right ) {
    const TerraAABB* left_aabb = & ( ( const TerraBVHVolume* ) left )->aabb;
    const TerraAABB* right_aabb = & ( ( const TerraBVHVolume* ) right )->aabb;

    if ( terra_aabb_center ( left_aabb ).y < terra_aabb_center ( right_aabb ).y ) {
        return true;
    }

    return false;
}

int terra_bvh_volume_compare_z ( const void* left, const void* right ) {
    const TerraAABB* left_aabb = & ( ( const TerraBVHVolume* ) left )->aabb;
    const TerraAABB* right_aabb = & ( ( const TerraBVHVolume* ) right )->aabb;

    if ( terra_aabb_center ( left_aabb ).z < terra_aabb_center ( right_aabb ).z ) {
        return true;
    }

    return false;
}

void terra_aabb_fit_aabb ( TerraAABB* aabb, const TerraAABB* other ) {
    aabb->min.x = terra_minf ( aabb->min.x, other->min.x );
    aabb->min.y = terra_minf ( aabb->min.y, other->min.y );
    aabb->min.z = terra_minf ( aabb->min.z, other->min.z );
    aabb->max.x = terra_maxf ( aabb->max.x, other->max.x ) + terra_Epsilon;
    aabb->max.y = terra_maxf ( aabb->max.y, other->max.y ) + terra_Epsilon;
    aabb->max.z = terra_maxf ( aabb->max.z, other->max.z ) + terra_Epsilon;
}

int terra_bvh_sah_split_volumes ( TerraBVHVolume* volumes, int volumes_count, const TerraAABB* container ) {
    float* left_area = ( float* ) terra_malloc ( sizeof ( *left_area ) * ( volumes_count - 1 ) );
    float* right_area = ( float* ) terra_malloc ( sizeof ( *right_area ) * ( volumes_count - 1 ) );
    float container_area;

    if ( container != NULL ) {
        container_area = terra_aabb_surface_area ( container );
    } else {
        container_area = FLT_MAX;
    }

    float min_cost = FLT_MAX;//volumes_count;
    int min_cost_idx = -1;
    // TODO for other axis
    qsort ( volumes, volumes_count, sizeof ( *volumes ), terra_bvh_volume_compare_x );
    TerraAABB aabb;
    aabb.min = terra_f3_set1 ( FLT_MAX );
    aabb.max = terra_f3_set1 ( -FLT_MAX );

    for ( int i = 0; i < volumes_count - 1; ++i ) {
        terra_aabb_fit_aabb ( &aabb, &volumes[i].aabb );
        left_area[i] = terra_aabb_surface_area ( &aabb );
    }

    aabb.min = terra_f3_set1 ( FLT_MAX );
    aabb.max = terra_f3_set1 ( -FLT_MAX );

    for ( int i = volumes_count - 1; i > 0; --i ) {
        terra_aabb_fit_aabb ( &aabb, &volumes[i].aabb );
        right_area[i - 1] = terra_aabb_surface_area ( &aabb );
    }

    for ( int i = 0; i < volumes_count - 1; ++i ) {
        int left_count = i + 1;
        int right_count = volumes_count - left_count;
        // we assume traversal_step_cost is 0 and intersection_test_cost is 1
        float cost = left_count * left_area[i] / container_area + right_count * right_area[i] / container_area;

        if ( cost < min_cost ) {
            min_cost = cost;
            min_cost_idx = i;
        }
    }

    free ( left_area );
    free ( right_area );
    return min_cost_idx;
}

void terra_bvh_create ( TerraBVH* bvh, const TerraObject* objects, int objects_count ) {
    // init the scene aabb and the list of volumes
    // a volume is a scene primitive (triangle) wrapped in an aabb
    TerraAABB scene_aabb;
    scene_aabb.min = terra_f3_set1 ( FLT_MAX );
    scene_aabb.max = terra_f3_set1 ( -FLT_MAX );
    int volumes_count = 0;

    for ( int i = 0; i < objects_count; ++i ) {
        volumes_count += objects[i].triangles_count;
    }

    TerraBVHVolume* volumes = ( TerraBVHVolume* ) terra_malloc ( sizeof ( *volumes ) * volumes_count );

    for ( int i = 0; i < volumes_count; ++i ) {
        volumes[i].aabb.min = terra_f3_set1 ( FLT_MAX );
        volumes[i].aabb.max = terra_f3_set1 ( -FLT_MAX );
    }

    int p = 0;

    // build the scene aabb and the volumes aabb
    for ( int j = 0; j < objects_count; ++j ) {
        for ( int i = 0; i < objects[j].triangles_count; ++i, ++p ) {
            terra_aabb_fit_triangle ( &scene_aabb, &objects[j].triangles[i] );
            terra_aabb_fit_triangle ( &volumes[p].aabb, &objects[j].triangles[i] );
            volumes[p].type = 1;
            volumes[p].index = j | ( i << 8 );
        }
    }

    // build the bvh. we do iterative building using a stack
    bvh->nodes = ( TerraBVHNode* ) terra_malloc ( sizeof ( *bvh->nodes ) * volumes_count * 2 );
    bvh->nodes_count = 1;
    // a stack task holds the idx of the node to be created along with its aabb
    // and the volumes it holds
    typedef struct {
        int volumes_start;
        int volumes_end;
        int node_idx;
        TerraAABB* aabb;
    } StackTask;
    StackTask* stack = ( StackTask* ) malloc ( sizeof ( *stack ) * volumes_count * 2 );
    int stack_idx = 0;
    stack[stack_idx].volumes_start = 0;
    stack[stack_idx].volumes_end = volumes_count;
    stack[stack_idx].node_idx = 0;
    stack[stack_idx].aabb = &scene_aabb;
    ++stack_idx;

    while ( stack_idx > 0 ) {
        // split the volumes in two
        StackTask t = stack[--stack_idx];
        int split_idx = terra_bvh_sah_split_volumes ( volumes + t.volumes_start, t.volumes_end - t.volumes_start, t.aabb ) + t.volumes_start;
        // left child
        TerraBVHNode* node = &bvh->nodes[t.node_idx];

        if ( split_idx == t.volumes_start ) {
            // only one volume in this branch, therefore it's a leaf and we're done here
            node->type[0] = volumes[t.volumes_start].type;
            node->aabb[0] = volumes[t.volumes_start].aabb;
            node->index[0] = volumes[t.volumes_start].index;
        } else {
            // more than one volumes, therefore more splitting is needed
            node->type[0] = -1;
            TerraAABB aabb;
            aabb.min = terra_f3_set1 ( FLT_MAX );
            aabb.max = terra_f3_set1 ( -FLT_MAX );

            for ( int i = t.volumes_start; i < split_idx + 1; ++i ) {
                terra_aabb_fit_aabb ( &aabb, &volumes[i].aabb );
            }

            node->aabb[0] = aabb;
            node->index[0] = bvh->nodes_count;
            // create a new stack task
            stack[stack_idx].volumes_start = t.volumes_start;
            stack[stack_idx].volumes_end = split_idx + 1;
            stack[stack_idx].aabb = &node->aabb[0];
            stack[stack_idx].node_idx = bvh->nodes_count;
            ++stack_idx;
            ++bvh->nodes_count;
        }

        // right child
        node = &bvh->nodes[t.node_idx];

        if ( split_idx == t.volumes_end - 2 ) {
            // only one volume in this branch, therefore it's a leaf and we're done here
            node->type[1] = volumes[t.volumes_end - 1].type;
            node->aabb[1] = volumes[t.volumes_end - 1].aabb;
            node->index[1] = volumes[t.volumes_end - 1].index;
        } else {
            // more than one volumes, therefore more splitting is needed
            node->type[1] = -1;
            TerraAABB aabb;
            aabb.min = terra_f3_set1 ( FLT_MAX );
            aabb.max = terra_f3_set1 ( -FLT_MAX );

            for ( int i = split_idx + 1; i < t.volumes_end; ++i ) {
                terra_aabb_fit_aabb ( &aabb, &volumes[i].aabb );
            }

            node->aabb[1] = aabb;
            node->index[1] = bvh->nodes_count;
            // create a new stack task
            stack[stack_idx].volumes_start = split_idx + 1;
            stack[stack_idx].volumes_end = t.volumes_end;
            stack[stack_idx].aabb = &node->aabb[1];
            stack[stack_idx].node_idx = bvh->nodes_count;
            ++stack_idx;
            ++bvh->nodes_count;
        }
    }
}

void terra_bvh_destroy ( TerraBVH* bvh ) {
    terra_free ( bvh->nodes );
}

bool terra_bvh_traverse ( TerraBVH* bvh, const TerraObject* objects, const TerraRay* ray, const TerraRayState* ray_state,
                          TerraFloat3* point_out, TerraPrimitiveRef* primitive_out ) {
    int queue[64];
    queue[0] = 0;
    int queue_count = 1;
    int node = 0;
    float min_d = FLT_MAX;
    TerraFloat3 min_p = terra_f3_set1 ( FLT_MAX );
    bool found = false;

    // Intersection queries (already initialized)
    TerraRayIntersectionResult iset_result;
    TerraRayIntersectionQuery  iset_query;
    iset_query.ray = ray;
    iset_query.state = ray_state;

    while ( queue_count > 0 ) {
        node = queue[--queue_count];

        for ( int i = 0; i < 2; ++i ) {
            switch ( bvh->nodes[node].type[i] ) {
                case -1:

                    // not leaf
                    if ( terra_ray_aabb_intersection ( ray, &bvh->nodes[node].aabb[i], NULL, NULL ) ) {
                        queue[queue_count++] = bvh->nodes[node].index[i];
                    }

                    break;

                case 1:
                    // triangle
                {
                    int model_idx = bvh->nodes[node].index[i] & 0xff;
                    int tri_idx = bvh->nodes[node].index[i] >> 8;

                    if ( model_idx == 0 ) {
                        //printf ( "moo" );
                    }

                    if ( model_idx == 1 && tri_idx == 0 ) {
                        //printf ( "moo" );
                    }

                    iset_query.primitive.triangle = objects[model_idx].triangles + tri_idx;

                    if ( terra_ray_triangle_intersection_query ( &iset_query, &iset_result ) ) {
                        // Is it within the bounds ?
                        if ( iset_result.ray_depth < min_d ) {
                            min_d = iset_result.ray_depth;
                            min_p = iset_result.point;
                            primitive_out->object_idx = model_idx;
                            primitive_out->triangle_idx = tri_idx;
                            found = true;
                        }
                    }
                }
                break;

                default:
                    assert ( false );
                    break;
            }
        }
    }

    *point_out = min_p;
    return found;
}
