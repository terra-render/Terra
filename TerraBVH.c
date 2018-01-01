// Header
#include "TerraBVH.h"

float terra_aabb_surface_area(const TerraAABB* aabb)
{
    float w = aabb->max.x - aabb->min.x;
    float h = aabb->max.y - aabb->min.y;
    float d = aabb->max.z - aabb->min.z;
    return 2 * (w*d + w*h + d*h);
}

TerraFloat3 terra_aabb_center(const TerraAABB* aabb)
{
    TerraFloat3 center = terra_addf3(&aabb->min, &aabb->max);
    center = terra_divf3(&center, 2);
    return center;
}

int terra_bvh_volume_compare_x(const void* left, const void* right)
{
    const TerraAABB* left_aabb = &((const TerraBVHVolume*)left)->aabb;
    const TerraAABB* right_aabb = &((const TerraBVHVolume*)right)->aabb;
    if (terra_aabb_center(left_aabb).x < terra_aabb_center(right_aabb).x) return true;
    return false;
}

int terra_bvh_volume_compare_y(const void* left, const void* right)
{
    const TerraAABB* left_aabb = &((const TerraBVHVolume*)left)->aabb;
    const TerraAABB* right_aabb = &((const TerraBVHVolume*)right)->aabb;
    if (terra_aabb_center(left_aabb).y < terra_aabb_center(right_aabb).y) return true;
    return false;
}

int terra_bvh_volume_compare_z(const void* left, const void* right)
{
    const TerraAABB* left_aabb = &((const TerraBVHVolume*)left)->aabb;
    const TerraAABB* right_aabb = &((const TerraBVHVolume*)right)->aabb;
    if (terra_aabb_center(left_aabb).z < terra_aabb_center(right_aabb).z) return true;
    return false;
}

void terra_aabb_fit_triangle(TerraAABB* aabb, const TerraTriangle* triangle)
{
    aabb->min.x = terra_minf(aabb->min.x, triangle->a.x);
    aabb->min.x = terra_minf(aabb->min.x, triangle->b.x);
    aabb->min.x = terra_minf(aabb->min.x, triangle->c.x);
    aabb->min.y = terra_minf(aabb->min.y, triangle->a.y);
    aabb->min.y = terra_minf(aabb->min.y, triangle->b.y);
    aabb->min.y = terra_minf(aabb->min.y, triangle->c.y);
    aabb->min.z = terra_minf(aabb->min.z, triangle->a.z);
    aabb->min.z = terra_minf(aabb->min.z, triangle->b.z);
    aabb->min.z = terra_minf(aabb->min.z, triangle->c.z);
    aabb->min.x -= terra_Epsilon;
    aabb->min.y -= terra_Epsilon;
    aabb->min.z -= terra_Epsilon;

    aabb->max.x = terra_maxf(aabb->max.x, triangle->a.x);
    aabb->max.x = terra_maxf(aabb->max.x, triangle->b.x);
    aabb->max.x = terra_maxf(aabb->max.x, triangle->c.x);
    aabb->max.y = terra_maxf(aabb->max.y, triangle->a.y);
    aabb->max.y = terra_maxf(aabb->max.y, triangle->b.y);
    aabb->max.y = terra_maxf(aabb->max.y, triangle->c.y);
    aabb->max.z = terra_maxf(aabb->max.z, triangle->a.z);
    aabb->max.z = terra_maxf(aabb->max.z, triangle->b.z);
    aabb->max.z = terra_maxf(aabb->max.z, triangle->c.z);
    aabb->max.x += terra_Epsilon;
    aabb->max.y += terra_Epsilon;
    aabb->max.z += terra_Epsilon;
}

TerraFloat3 terra_read_texture(const TerraTexture* texture, int x, int y)
{
    switch (texture->address_mode)
    {
    case kTerraTextureAddressClamp:
        x = (int)terra_minf((float)x, (float)(texture->width - 1));
        y = (int)terra_minf((float)y, (float)(texture->height - 1));
        break;
    case kTerraTextureAddressWrap:
        x = x % texture->width;
        y = y % texture->height;
        break;
        // TODO: Faster implementation
    case kTerraTextureAddressMirror:
        if ((int)(x / texture->width) % 2 == 0)
        {
            x = x % texture->width;
            y = y % texture->height;
        }
        else
        {
            x = texture->width - (x % texture->width);
            y = texture->height - (y % texture->height);
        }
        break;
    }

    uint8_t* pixel = &texture->pixels[(y * texture->width + x) * texture->comps + texture->offset];
    return terra_f3_set((float)pixel[0] / 255, (float)pixel[1] / 255, (float)pixel[2] / 255);
}

TerraFloat3 terra_read_hdr_texture(const TerraHDRTexture* texture, int x, int y)
{
    // HDR textures are RGB
    float* pixel = &texture->pixels[(y * texture->width + x) * 3];
    return *(TerraFloat3*)pixel;
}

TerraFloat3 terra_sample_texture(const TerraTexture* texture, const TerraFloat2* uv)
{
    // https://en.wikipedia.org/wiki/Bilinear_filtering
    const TerraFloat2 mapped_uv = terra_f2_set(uv->x * texture->width - 0.5f, uv->y * texture->height - 0.5f);

    const int ix = (int)mapped_uv.x;
    const int iy = (int)mapped_uv.y;

    TerraFloat3 final_color;
    switch (texture->filter)
    {
        // Closest
    case kTerraFilterPoint:
    {
        final_color = terra_read_texture(texture, ix, iy);
        break;
    }
    // Standard bilinear filtering. Averages neighbor pixels weighting them. No mipmapping
    case kTerraFilterBilinear:
    {
        // TL
        int x1 = ix;
        int y1 = iy;

        // TR
        int x2 = terra_mini(ix + 1, texture->width - 1);
        int y2 = iy;

        // BL
        int x3 = ix;
        int y3 = terra_mini(iy + 1, texture->height - 1);

        // BR
        int x4 = terra_mini(ix + 1, texture->width - 1);
        int y4 = terra_mini(iy + 1, texture->height - 1);

        TerraFloat3 n1 = terra_read_texture(texture, x1, y1);
        TerraFloat3 n2 = terra_read_texture(texture, x2, y2);
        TerraFloat3 n3 = terra_read_texture(texture, x3, y3);
        TerraFloat3 n4 = terra_read_texture(texture, x4, y4);

        const float w_u = mapped_uv.x - ix;
        const float w_v = mapped_uv.y - iy;
        const float w_ou = 1.f - w_u;
        const float w_ov = 1.f - w_v;

        final_color.x = (n1.x * w_ou + n2.x * w_u) * w_ov + (n3.x * w_ou + n4.x * w_u) * w_v;
        final_color.y = (n1.y * w_ou + n2.y * w_u) * w_ov + (n3.y * w_ou + n4.y * w_u) * w_v;
        final_color.z = (n1.z * w_ou + n2.z * w_u) * w_ov + (n3.z * w_ou + n4.z * w_u) * w_v;
        break;
    }
    }

    return final_color;
}

TerraFloat3 terra_sample_hdr_cubemap(const TerraHDRTexture* texture, const TerraFloat3* dir)
{
    TerraFloat3 v = terra_normf3(dir);
    float theta = acosf(v.y);
    float phi = atan2f(v.z, v.x) + terra_PI;
    TerraFloat2 mapped_uv;
    mapped_uv.x = (phi / (2 * terra_PI)) * texture->width;
    mapped_uv.y = (theta / terra_PI) * texture->height;

    const int ix = (int)mapped_uv.x;
    const int iy = (int)mapped_uv.y;

    int x1 = ix;
    int y1 = iy;

    int x2 = terra_mini(ix + 1, texture->width - 1);
    int y2 = iy;

    int x3 = ix;
    int y3 = terra_mini(iy + 1, texture->height - 1);

    int x4 = terra_mini(ix + 1, texture->width - 1);
    int y4 = terra_mini(iy + 1, texture->height - 1);

    TerraFloat3 n1 = terra_read_hdr_texture(texture, x1, y1);
    TerraFloat3 n2 = terra_read_hdr_texture(texture, x2, y2);
    TerraFloat3 n3 = terra_read_hdr_texture(texture, x3, y3);
    TerraFloat3 n4 = terra_read_hdr_texture(texture, x4, y4);

    const float w_u = mapped_uv.x - ix;
    const float w_v = mapped_uv.y - iy;
    const float w_ou = 1.f - w_u;
    const float w_ov = 1.f - w_v;

    TerraFloat3 final_color;
    final_color.x = (n1.x * w_ou + n2.x * w_u) * w_ov + (n3.x * w_ou + n4.x * w_u) * w_v;
    final_color.y = (n1.y * w_ou + n2.y * w_u) * w_ov + (n3.y * w_ou + n4.y * w_u) * w_v;
    final_color.z = (n1.z * w_ou + n2.z * w_u) * w_ov + (n3.z * w_ou + n4.z * w_u) * w_v;
    return final_color;
}

TerraFloat3 terra_eval_attribute(const TerraAttribute* attribute, const TerraFloat2* uv)
{
    if (attribute->map.pixels != NULL)
        return terra_sample_texture(&attribute->map, uv);
    return attribute->value;
}

void terra_aabb_fit_aabb(TerraAABB* aabb, const TerraAABB* other)
{
    aabb->min.x = terra_minf(aabb->min.x, other->min.x);
    aabb->min.y = terra_minf(aabb->min.y, other->min.y);
    aabb->min.z = terra_minf(aabb->min.z, other->min.z);

    aabb->max.x = terra_maxf(aabb->max.x, other->max.x) + terra_Epsilon;
    aabb->max.y = terra_maxf(aabb->max.y, other->max.y) + terra_Epsilon;
    aabb->max.z = terra_maxf(aabb->max.z, other->max.z) + terra_Epsilon;
}

int terra_bvh_sah_split_volumes(TerraBVHVolume* volumes, int volumes_count, const TerraAABB* container)
{
    float* left_area = (float*)terra_malloc(sizeof(*left_area) * (volumes_count - 1));
    float* right_area = (float*)terra_malloc(sizeof(*right_area) * (volumes_count - 1));
    float container_area;
    if (container != NULL)
        container_area = terra_aabb_surface_area(container);
    else
        container_area = FLT_MAX;
    float min_cost = FLT_MAX;//volumes_count;
    int min_cost_idx = -1;

    // TODO for other axis
    qsort(volumes, volumes_count, sizeof(*volumes), terra_bvh_volume_compare_x);

    TerraAABB aabb;
    aabb.min = terra_f3_set1(FLT_MAX);
    aabb.max = terra_f3_set1(-FLT_MAX);
    for (int i = 0; i < volumes_count - 1; ++i)
    {
        terra_aabb_fit_aabb(&aabb, &volumes[i].aabb);
        left_area[i] = terra_aabb_surface_area(&aabb);
    }

    aabb.min = terra_f3_set1(FLT_MAX);
    aabb.max = terra_f3_set1(-FLT_MAX);
    for (int i = volumes_count - 1; i > 0; --i)
    {
        terra_aabb_fit_aabb(&aabb, &volumes[i].aabb);
        right_area[i - 1] = terra_aabb_surface_area(&aabb);
    }

    for (int i = 0; i < volumes_count - 1; ++i)
    {
        int left_count = i + 1;
        int right_count = volumes_count - left_count;
        // we assume traversal_step_cost is 0 and intersection_test_cost is 1
        float cost = left_count * left_area[i] / container_area + right_count * right_area[i] / container_area;
        if (cost < min_cost)
        {
            min_cost = cost;
            min_cost_idx = i;
        }
    }

    free(left_area);
    free(right_area);

    return min_cost_idx;
}

void terra_bvh_create(TerraBVH* bvh, const TerraScene* scene)
{
    // init the scene aabb and the list of volumes
    // a volume is a scene primitive (triangle) wrapped in an aabb
    TerraAABB scene_aabb;
    scene_aabb.min = terra_f3_set1(FLT_MAX);
    scene_aabb.max = terra_f3_set1(-FLT_MAX);
    int volumes_count = 0;
    for (int i = 0; i < scene->objects_count; ++i)
    {
        volumes_count += scene->objects[i].triangles_count;
    }

    TerraBVHVolume* volumes = (TerraBVHVolume*)terra_malloc(sizeof(*volumes) * volumes_count);
    for (int i = 0; i < volumes_count; ++i)
    {
        volumes[i].aabb.min = terra_f3_set1(FLT_MAX);
        volumes[i].aabb.max = terra_f3_set1(-FLT_MAX);
    }
    int p = 0;

    // build the scene aabb and the volumes aabb
    for (int j = 0; j < scene->objects_count; ++j)
    {
        for (int i = 0; i < scene->objects[j].triangles_count; ++i, ++p)
        {
            terra_aabb_fit_triangle(&scene_aabb, &scene->objects[j].triangles[i]);
            terra_aabb_fit_triangle(&volumes[p].aabb, &scene->objects[j].triangles[i]);
            volumes[p].type = 1;
            volumes[p].index = j | (i << 8);
        }
    }

    // build the bvh. we do iterative building using a stack
    bvh->nodes = (TerraBVHNode*)terra_malloc(sizeof(*bvh->nodes) * volumes_count * 2);
    bvh->nodes_count = 1;

    // a stack task holds the idx of the node to be created along with its aabb
    // and the volumes it holds
    typedef struct
    {
        int volumes_start;
        int volumes_end;
        int node_idx;
        TerraAABB* aabb;
    } StackTask;

    StackTask* stack = (StackTask*)malloc(sizeof(*stack) * volumes_count * 2);
    int stack_idx = 0;

    stack[stack_idx].volumes_start = 0;
    stack[stack_idx].volumes_end = volumes_count;
    stack[stack_idx].node_idx = 0;
    stack[stack_idx].aabb = &scene_aabb;
    ++stack_idx;

    while (stack_idx > 0) 
    {
        // split the volumes in two
        StackTask t = stack[--stack_idx];
        int split_idx = terra_bvh_sah_split_volumes(volumes + t.volumes_start, t.volumes_end - t.volumes_start, t.aabb) + t.volumes_start;

        // left child
        TerraBVHNode* node = &bvh->nodes[t.node_idx];
        if (split_idx == t.volumes_start)
        {
            // only one volume in this branch, therefore it's a leaf and we're done here
            node->type[0] = volumes[t.volumes_start].type;
            node->aabb[0] = volumes[t.volumes_start].aabb;
            node->index[0] = volumes[t.volumes_start].index;
        }
        else {
            // more than one volumes, therefore more splitting is needed
            node->type[0] = -1;
            TerraAABB aabb;
            aabb.min = terra_f3_set1(FLT_MAX);
            aabb.max = terra_f3_set1(-FLT_MAX);
            for (int i = t.volumes_start; i < split_idx + 1; ++i)
            {
                terra_aabb_fit_aabb(&aabb, &volumes[i].aabb);
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
        if (split_idx == t.volumes_end - 2)
        {
            // only one volume in this branch, therefore it's a leaf and we're done here
            node->type[1] = volumes[t.volumes_end - 1].type;
            node->aabb[1] = volumes[t.volumes_end - 1].aabb;
            node->index[1] = volumes[t.volumes_end - 1].index;
        }
        else
        {
            // more than one volumes, therefore more splitting is needed
            node->type[1] = -1;
            TerraAABB aabb;
            aabb.min = terra_f3_set1(FLT_MAX);
            aabb.max = terra_f3_set1(-FLT_MAX);
            for (int i = split_idx + 1; i < t.volumes_end; ++i)
            {
                terra_aabb_fit_aabb(&aabb, &volumes[i].aabb);
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

void terra_bvh_destroy(TerraBVH* bvh)
{
    terra_free(bvh->nodes);
}

bool terra_bvh_traverse(TerraBVH* bvh, const TerraRay* ray, const TerraScene* scene, TerraFloat3* point_out, TerraPrimitiveRef* primitive_out)
{
    int queue[64];
    queue[0] = 0;
    int queue_count = 1;

    int node = 0;

    float min_d = FLT_MAX;
    TerraFloat3 min_p = terra_f3_set1(FLT_MAX);

    bool found = false;
    while (queue_count > 0)
    {
        node = queue[--queue_count];
        for (int i = 0; i < 2; ++i)
        {
            switch (bvh->nodes[node].type[i])
            {
            case -1:
                // not leaf
                if (terra_ray_aabb_intersection(ray, &bvh->nodes[node].aabb[i], NULL, NULL))
                {
                    queue[queue_count++] = bvh->nodes[node].index[i];
                }
                break;
            case 1:
                // triangle
            {
                int model_idx = bvh->nodes[node].index[i] & 0xff;
                int tri_idx = bvh->nodes[node].index[i] >> 8;
                TerraFloat3 p;
                if (terra_ray_triangle_intersection(ray, &scene->objects[model_idx].triangles[tri_idx], &p, NULL))
                {
                    TerraFloat3 po = terra_subf3(&p, &ray->origin);
                    float d = terra_lenf3(&po);
                    if (d < min_d)
                    {
                        min_d = d;
                        min_p = p;
                        primitive_out->object_idx = model_idx;
                        primitive_out->triangle_idx = tri_idx;
                        found = true;
                    }
                }
            }
            break;
            default:
                assert(false);
                break;
            }
        }
    }
    *point_out = min_p;
    return found;
}
