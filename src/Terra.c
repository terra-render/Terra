// Header
#include <Terra.h>

// Terra
#include "TerraBVH.h"
#include "TerraKDTree.h"

//--------------------------------------------------------------------------------------------------
// Terra internal types
//--------------------------------------------------------------------------------------------------
#define TERRA_DO_RUSSIAN_ROULETTE
#define TERRA_DO_DIRECT_LIGHTING
//--------------------------------------------------------------------------------------------------
// Terra internal types
//--------------------------------------------------------------------------------------------------
typedef struct TerraSceneImpl
{
	TerraKDTree kdtree;
	TerraBVH bvh;
}TerraSceneImpl;
#define _impl(scene) ((TerraSceneImpl*)((scene)->_impl))

//--------------------------------------------------------------------------------------------------
// Terra internal routines
//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_trace(TerraScene* scene, const TerraRay* ray);
TerraFloat3 terra_get_pixel_pos(const TerraCamera *camera, const TerraFramebuffer *frame, int x, int y, float half_range);
void        terra_triangle_init_shading(const TerraTriangle* triangle, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingContext* ctx_out);

TerraFloat3 terra_read_hdr_texture(const TerraHDRTexture* texture, int x, int y);
TerraFloat3 terra_read_texture(const TerraTexture* texture, int x, int y);

//--------------------------------------------------------------------------------------------------
// Terra implementation
//--------------------------------------------------------------------------------------------------
#if 0
#include <immintrin.h>
__m128 terra_sse_loadf3(const TerraFloat3* xyz)
{
    __m128 x = _mm_load_ss(&xyz->x);
    __m128 y = _mm_load_ss(&xyz->y);
    __m128 z = _mm_load_ss(&xyz->z);
    __m128 xy = _mm_movelh_ps(x, y);
    return _mm_shuffle_ps(xy, z, _MM_SHUFFLE(2, 0, 2, 0));
}

__m128 terra_sse_cross(__m128 a, __m128 b)
{
    return _mm_sub_ps(
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2))),
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)))
    );
}

float terra_sse_dotf3(__m128 a, __m128 b)
{
    __m128 dp = _mm_dp_ps(a, b, 0xFF);
    return *(float*)&dp;
}
#endif

bool terra_ray_triangle_intersection(const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out, float* t_out)
{
    const TerraTriangle *tri = triangle;
#if 1
    TerraFloat3 e1, e2, h, s, q;
    float a, f, u, v, t;

    e1 = terra_subf3(&tri->b, &tri->a);
    e2 = terra_subf3(&tri->c, &tri->a);

    h = terra_crossf3(&ray->direction, &e2);
    a = terra_dotf3(&e1, &h);
    if (a > -terra_Epsilon && a < terra_Epsilon)
        return false;
    
    f = 1 / a;
    s = terra_subf3(&ray->origin, &tri->a);
    u = f * (terra_dotf3(&s, &h));
    if (u < 0.0 || u > 1.0)
        return false;

    q = terra_crossf3(&s, &e1);
    v = f * terra_dotf3(&ray->direction, &q);
    if (v < 0.0 || u + v > 1.0)
        return false;

    t = f * terra_dotf3(&e2, &q);

    if (t > 0.00001)
    {
        TerraFloat3 offset = terra_mulf3(&ray->direction, t);
        *point_out = terra_addf3(&offset, &ray->origin);
        if (t_out != NULL) *t_out = t;
        return true;
    }
    return false;
#else
    __m128 tri_a = terra_sse_loadf3(&tri->a);
    __m128 tri_b = terra_sse_loadf3(&tri->b);
    __m128 tri_c = terra_sse_loadf3(&tri->c);
    __m128 ray_ori = terra_sse_loadf3(&ray->origin);
    __m128 ray_dir = terra_sse_loadf3(&ray->direction);

    __m128 e1, e2, h, s, q;
    float a, f, u, v, t;

    e1 = _mm_sub_ps(tri_b, tri_a);
    e2 = _mm_sub_ps(tri_c, tri_a);

    h = terra_sse_cross(ray_dir, e2);
    a = terra_sse_dotf3(e1, h);

    if (a > -terra_Epsilon && a < terra_Epsilon)
        return false;

    f = 1.f / a;
    s = _mm_sub_ps(ray_ori, tri_a);
    u = f * terra_sse_dotf3(s, h);
    if (u < 0.f || u > 1.f)
        return false;

    q = terra_sse_cross(s, e1);
    v = f * terra_sse_dotf3(ray_dir, q);
    if (v < 0.f || u + v > 1.f)
        return false;

    t = f * terra_sse_dotf3(e2, q);

    if (t > 0.00001)
    {
        TerraFloat3 offset = terra_mulf3(&ray->direction, t);
        *point_out = terra_addf3(&offset, &ray->origin);
        if (t_out != NULL) *t_out = t;
        return true;
    }
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool terra_ray_aabb_intersection(const TerraRay *ray, const TerraAABB *aabb, float* tmin_out, float* tmax_out)
{
    float t1 = (aabb->min.x - ray->origin.x) * ray->inv_direction.x;
    float t2 = (aabb->max.x - ray->origin.x) * ray->inv_direction.x;

    float tmin = terra_minf(t1, t2);
    float tmax = terra_maxf(t1, t2);

    t1 = (aabb->min.y - ray->origin.y) * ray->inv_direction.y;
    t2 = (aabb->max.y - ray->origin.y) * ray->inv_direction.y;

    tmin = terra_maxf(tmin, terra_minf(t1, t2));
    tmax = terra_minf(tmax, terra_maxf(t1, t2));

    t1 = (aabb->min.z - ray->origin.z) * ray->inv_direction.z;
    t2 = (aabb->max.z - ray->origin.z) * ray->inv_direction.z;

    tmin = terra_maxf(tmin, terra_minf(t1, t2));
    tmax = terra_minf(tmax, terra_maxf(t1, t2));

    if (tmax > terra_maxf(tmin, 0.f))
    {
        if (tmin_out != NULL) *tmin_out = tmin;
        if (tmax_out != NULL) *tmax_out = tmax;
        return true;
    }
    return false;
}
//--------------------------------------------------------------------------------------------------
void terra_triangle_init_shading(const TerraTriangle* triangle, const TerraTriangleProperties* properties, const TerraFloat3* point, TerraShadingContext* ctx)
{
    TerraFloat3 e0 = terra_subf3(&triangle->b, &triangle->a);
    TerraFloat3 e1 = terra_subf3(&triangle->c, &triangle->a);
    TerraFloat3 p = terra_subf3(point, &triangle->a);

    float d00 = terra_dotf3(&e0, &e0);
    float d11 = terra_dotf3(&e1, &e1);
    float d01 = terra_dotf3(&e0, &e1);
    float dp0 = terra_dotf3(&p, &e0);
    float dp1 = terra_dotf3(&p, &e1);

    float div = d00 * d11 - d01 * d01;

    TerraFloat2 uv;
    uv.x = (d11 * dp0 - d01 * dp1) / div;
    uv.y = (d00 * dp1 - d01 * dp0) / div;

    TerraFloat3 na = terra_mulf3(&properties->normal_c, uv.y);
    TerraFloat3 nb = terra_mulf3(&properties->normal_b, uv.x);
    TerraFloat3 nc = terra_mulf3(&properties->normal_a, 1 - uv.x - uv.y);

    ctx->normal = terra_addf3(&na, &nb);
    ctx->normal = terra_addf3(&ctx->normal, &nc);
    ctx->normal = terra_normf3(&ctx->normal);

    TerraFloat2 ta = terra_mulf2(&properties->texcoord_c, uv.y);
    TerraFloat2 tb = terra_mulf2(&properties->texcoord_b, uv.x);
    TerraFloat2 tc = terra_mulf2(&properties->texcoord_a, 1 - uv.x - uv.y);

    ctx->texcoord = terra_addf2(&ta, &tb);
    ctx->texcoord = terra_addf2(&ctx->texcoord, &tc);
    ctx->texcoord = ctx->texcoord;
}

//--------------------------------------------------------------------------------------------------
void terra_scene_begin(TerraScene* scene, int objects_count, int materials_count)
{
    scene->objects = (TerraObject*)terra_malloc(sizeof(TerraObject) * objects_count);
    scene->objects_count = 0;
    scene->lights = (TerraLight*)terra_malloc(sizeof(TerraLight) * objects_count);
    scene->lights_count = 0;
    scene->opts.environment_map.pixels = NULL;
    scene->_impl = NULL;
}

//--------------------------------------------------------------------------------------------------
TerraObject* terra_scene_add_object(TerraScene* scene)
{
    return &scene->objects[scene->objects_count++];
}

//--------------------------------------------------------------------------------------------------
void terra_prepare_texture(TerraTexture* texture)
{
    if (texture != NULL && texture->pixels != NULL && texture->srgb)
    {
        for (int i = 0; i < texture->width * texture->height * texture->comps; ++i)
            texture->pixels[i] = (uint8_t)(powf((float)texture->pixels[i] / 255, 2.2f) * 255);
       
        // Avoids double conversion
        texture->srgb = false;
    }
}

void terra_scene_end(TerraScene* scene)
{
    // TODO: Transform to world position ?? 
    scene->_impl = terra_malloc(sizeof(TerraSceneImpl));

    if (scene->opts.accelerator == kTerraAcceleratorBVH)
        terra_bvh_create(&_impl(scene)->bvh, scene);
    else if (scene->opts.accelerator == kTerraAcceleratorKDTree)
        terra_kdtree_create(&_impl(scene)->kdtree, scene);

    // dem lights

    // Encoding all textures to linear
    for (int i = 0; i < scene->objects_count; ++i)
    {
        TerraMaterial* material = &scene->objects[i].material;
        
        // Unlikely that roughness and metalness will be in srgb ?
        terra_prepare_texture(&material->albedo.map);
        terra_prepare_texture(&material->emissive.map);
        terra_prepare_texture(&material->metalness.map);
        terra_prepare_texture(&material->roughness.map);
    }
}

void terra_scene_destroy(TerraScene * scene)
{
    if (scene->_impl != NULL)
        terra_free(scene->_impl);
    if (scene != NULL)
        terra_free(scene->objects);
}

//--------------------------------------------------------------------------------------------------
bool terra_framebuffer_create(TerraFramebuffer* framebuffer, int width, int height)
{
    if (width == 0 || height == 0)
        return false;

    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->pixels = (TerraFloat3*)terra_malloc(sizeof(TerraFloat3) * width * height);
    framebuffer->results = (TerraRawIntegrationResult*)terra_malloc(sizeof(TerraRawIntegrationResult) * width * height);

    for (int i = 0; i < width * height; ++i)
    {
        framebuffer->pixels[i] = terra_f3_zero;
        framebuffer->results[i].acc = terra_f3_zero;
        framebuffer->results[i].samples = 0;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void terra_framebuffer_destroy(TerraFramebuffer* framebuffer)
{
    if (framebuffer == NULL)
        return;

    terra_free(framebuffer->results);
    terra_free(framebuffer->pixels);
    terra_free(framebuffer);
}

//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_get_pixel_pos(const TerraCamera *camera, const TerraFramebuffer *frame, int x, int y, float half_range)
{
    const float dx = -half_range + 2 * (float)rand() / (float)(RAND_MAX)* half_range;
    const float dy = -half_range + 2 * (float)rand() / (float)(RAND_MAX)* half_range;

    // [0:1], y points down
    const float ndc_x = (x + 0.5f + dx) / frame->width;
    const float ndc_y = (y + 0.5f + dy) / frame->height;

    // [-1:1], y points up
    const float screen_x = 2 * ndc_x - 1;
    const float screen_y = 1 - 2 * ndc_y;

    const float aspect_ratio = (float)frame->width / (float)frame->height;
    // [-aspect_ratio * tan(fov/2):aspect_ratio * tan(fov/2)]
    const float camera_x = screen_x * aspect_ratio * (float)tan((camera->fov * 0.0174533f) / 2);
    const float camera_y = screen_y * (float)tan((camera->fov * 0.0174533f) / 2);

    return terra_f3_set(camera_x, camera_y, 1.f);
}

//--------------------------------------------------------------------------------------------------
void terra_build_rotation_around_normal(TerraShadingContext* ctx)
{
    TerraFloat3 normalt;
    TerraFloat3 normalbt;

    TerraFloat3* normal = &ctx->normal;

    if (fabs(normal->x) > fabs(normal->y))
    {
        normalt = terra_f3_set(normal->z, 0.f, -normal->x);
        normalt = terra_mulf3(&normalt, sqrtf(normal->x * normal->x + normal->z * normal->z));
    }
    else
    {
        normalt = terra_f3_set(0.f, -normal->z, normal->y);
        normalt = terra_mulf3(&normalt, sqrtf(normal->y * normal->y + normal->z * normal->z));
    }
    normalbt = terra_crossf3(normal, &normalt);

    ctx->rot.rows[0] = terra_f4(normalt.x, ctx->normal.x, normalbt.x, 0.f);
    ctx->rot.rows[1] = terra_f4(normalt.y, ctx->normal.y, normalbt.y, 0.f);
    ctx->rot.rows[2] = terra_f4(normalt.z, ctx->normal.z, normalbt.z, 0.f);
    ctx->rot.rows[3] = terra_f4(0.f, 0.f, 0.f, 1.f);
}

void terra_ray_create(TerraRay* ray, TerraFloat3* point, TerraFloat3* direction, TerraFloat3* normal, float sign)
{
    TerraFloat3 offset = terra_mulf3(normal, 0.0001f * sign);

    ray->origin = terra_addf3(point, &offset);
    ray->direction = *direction;

    ray->inv_direction.x = 1.f / ray->direction.x;
    ray->inv_direction.y = 1.f / ray->direction.y;
    ray->inv_direction.z = 1.f / ray->direction.z;
}

bool terra_find_closest(TerraScene* scene, const TerraRay* ray, const TerraMaterial** material_out, TerraShadingContext* ctx_out, TerraFloat3* intersection_point)
{
    TerraPrimitiveRef primitive;
    if (scene->opts.accelerator == kTerraAcceleratorBVH)
    {
        if (!terra_bvh_traverse(&_impl(scene)->bvh, ray, scene, intersection_point, &primitive))
            return false;
    }
    else if (scene->opts.accelerator == kTerraAcceleratorKDTree)
    {
        if (!terra_kdtree_traverse(&_impl(scene)->kdtree, ray, scene, intersection_point, &primitive))
            return false;
    }
    else
        return false;

    TerraObject* object = &scene->objects[primitive.object_idx];

    *material_out = &object->material;
    terra_triangle_init_shading(&object->triangles[primitive.triangle_idx], &object->properties[primitive.triangle_idx], intersection_point, ctx_out);
    return true;
}

TerraLight* terra_light_pick_power_proportional(const TerraScene* scene, float* e1) {
    // Compute total light emitted so that we can later weight
    // TODO cache this ?
    float total_light_power = 0;
    for (int i = 0; i < scene->lights_count; ++i) {
        total_light_power += scene->lights[i].emissive;
    }

    // Pick a light
    // e1 is a random float between 0 and 
    float alpha_acc = *e1;
    int light_idx = -1;
    for (int i = 0; i < scene->lights_count; ++i) {
        const float alpha = scene->lights[i].emissive / total_light_power;
        alpha_acc -= alpha;
        if (alpha_acc <= 0) {
            // We pick this light, save its index
            light_idx = i;
            // Recompute e1
            alpha_acc += alpha;
            *e1 = alpha_acc / alpha;
            break;
        }
    }
    assert(light_idx >= 0);
    return &scene->lights[light_idx];
}

float terra_light_pdf(const TerraLight* light, float distance) 
{
    return 1 / (terra_PI * light->radius * light->radius);
}

void terra_lookatf4x4(TerraFloat4x4* mat_out, const TerraFloat3* normal)
{
    TerraFloat3 normalt;
    TerraFloat3 normalbt;

    if (fabs(normal->x) > fabs(normal->y))
    {
        normalt = terra_f3_set(normal->z, 0.f, -normal->x);
        normalt = terra_mulf3(&normalt, sqrtf(normal->x * normal->x + normal->z * normal->z));
    }
    else
    {
        normalt = terra_f3_set(0.f, -normal->z, normal->y);
        normalt = terra_mulf3(&normalt, sqrtf(normal->y * normal->y + normal->z * normal->z));
    }
    normalbt = terra_crossf3(normal, &normalt);

    mat_out->rows[0] = terra_f4(normalt.x, normal->x, normalbt.x, 0.f);
    mat_out->rows[1] = terra_f4(normalt.y, normal->y, normalbt.y, 0.f);
    mat_out->rows[2] = terra_f4(normalt.z, normal->z, normalbt.z, 0.f);
    mat_out->rows[3] = terra_f4(0.f, 0.f, 0.f, 1.f);
}

TerraFloat3 terra_light_sample_disk(const TerraLight* light, const TerraFloat3* surface_point, float e1, float e2) {
    // pick an offset from the disk center
    TerraFloat3 disk_offset;
    disk_offset.x = light->radius * sqrtf(e1) * cosf(2 * terra_PI * e2);
    disk_offset.z = light->radius * sqrtf(e1) * sinf(2 * terra_PI * e2);
    disk_offset.y = 0;

    // direction from surface to picked light
    TerraFloat3 light_dir = terra_subf3(&light->center, surface_point); // TODO flip?
    light_dir = terra_normf3(&light_dir);

    // move the offset from the ideal flat disk to the actual bounding sphere section disk
    TerraFloat4x4 sample_rotation;
    terra_lookatf4x4(&sample_rotation, &light_dir);
    disk_offset = terra_transformf3(&sample_rotation, &disk_offset);

    // actual sample point and direction from surface
    TerraFloat3 sample_point = terra_addf3(&light->center, &disk_offset);
    TerraFloat3 sample_dir = terra_subf3(&sample_point, surface_point);
    //float sample_dist = terra_lenf3(&sample_dir);
    sample_dir = terra_normf3(&sample_dir);

    return sample_point;
}

TerraFloat3 terra_trace(TerraScene* scene, const TerraRay* primary_ray)
{
    TerraFloat3 Lo = terra_f3_zero;
    TerraFloat3 throughput = terra_f3_one;
    TerraRay ray = *primary_ray;

    for (int bounce = 0; bounce <= scene->opts.bounces; ++bounce)
    {
        const TerraMaterial* material;
        TerraShadingContext ctx;
        TerraFloat3 intersection_point;

        if (terra_find_closest(scene, &ray, &material, &ctx, &intersection_point) == false)
        {
            TerraFloat3 env_color;
            if (scene->opts.environment_map.pixels != NULL) {
                env_color = terra_sample_hdr_cubemap(&scene->opts.environment_map, &ray.direction);
            } else {
                env_color = terra_f3_zero;
            }
            throughput = terra_pointf3(&throughput, &env_color);
            Lo = terra_addf3(&Lo, &throughput);
            break;
        }

        terra_build_rotation_around_normal(&ctx);
        ctx.view = terra_negf3(&ray.direction);

        TerraFloat3 mat_emissive = terra_eval_attribute(&material->emissive, &ctx.texcoord);
        TerraFloat3 mat_albedo = terra_eval_attribute(&material->albedo, &ctx.texcoord);

        TerraFloat3 emissive = terra_mulf3(&throughput, mat_emissive.x);
        emissive = terra_pointf3(&emissive, &mat_albedo);
        Lo = terra_addf3(&Lo, &emissive);

        float e0 = (float)rand() / RAND_MAX;
        float e1 = (float)rand() / RAND_MAX;
        float e2 = (float)rand() / RAND_MAX;

        TerraShadingState state;
        TerraFloat3 bsdf_sample = material->bsdf.sample(material, &state, &ctx, e0, e1, e2);
        float       bsdf_pdf = terra_maxf(material->bsdf.weight(material, &state, &bsdf_sample, &ctx), terra_Epsilon);

        float light_pdf = 0.f;
#ifdef TERRA_DO_DIRECT_LIGHTING
        if (scene->opts.direct_sampling)
        {
            float l1 = (float)rand() / RAND_MAX;
            float l2 = (float)rand() / RAND_MAX;
            TerraLight* light = terra_light_pick_power_proportional(scene, &l1);

            TerraFloat3 light_sample_point = terra_light_sample_disk(light, &intersection_point, l1, l2);
            TerraFloat3 light_sample = terra_subf3(&light_sample_point, &intersection_point);
            float sample_dist = terra_lenf3(&light_sample);
            light_sample = terra_normf3(&light_sample);
            light_pdf = (terra_light_pdf(light, sample_dist), terra_Epsilon);
            float light_weight = light_pdf * light_pdf / (light_pdf * light_pdf + bsdf_pdf * bsdf_pdf);

            // Light contribution
            TerraFloat3 light_radiance = material->bsdf.shade(material, &state, &light_sample, &ctx);
            TerraFloat3 light_contribution = terra_mulf3(&light_radiance, light_weight / light_pdf);

            terra_ray_create(&ray, &intersection_point, &light_sample, &ctx.normal, 1.f);
            TerraShadingContext light_ctx;
            const TerraMaterial* light_material;
            TerraFloat3 light_intersection_point;
            if (terra_find_closest(scene, &ray, &light_material, &light_ctx, &light_intersection_point))
            {
                TerraFloat3 light_emissive = terra_eval_attribute(&light_material->emissive, &light_ctx.texcoord);
                light_emissive = terra_mulf3(&throughput, light_emissive.x);
                light_emissive = terra_pointf3(&light_emissive, &light_contribution);
                Lo = terra_addf3(&Lo, &light_emissive);
            }
        }
#endif
        // BSDF Contribution
        TerraFloat3 bsdf_radiance = material->bsdf.shade(material, &state, &bsdf_sample, &ctx);
        float bsdf_weight = bsdf_pdf * bsdf_pdf / (light_pdf * light_pdf + bsdf_pdf * bsdf_pdf);
        TerraFloat3 bsdf_contribution = terra_mulf3(&bsdf_radiance, bsdf_weight / bsdf_pdf);

        throughput = terra_pointf3(&throughput, &bsdf_contribution);
        
        // Russian roulette
#ifdef TERRA_DO_RUSSIAN_ROULETTE
        float p = terra_maxf(throughput.x, terra_maxf(throughput.y, throughput.z));
        float e3 = 0.5f;
        e3 = (float)rand() / RAND_MAX + terra_Epsilon;
        if (e3 > p)
            break;
        throughput = terra_mulf3(&throughput, 1.f / p);
#endif
        // Next ray (Skip if last?)
        float sNoL = terra_dotf3(&ctx.normal, &bsdf_sample);
        terra_ray_create(&ray, &intersection_point, &bsdf_sample, &ctx.normal, sNoL < 0.f ? -1.f : 1.f);
    }

    return Lo;
}

TerraFloat3 terra_tonemapping_uncharted2(const TerraFloat3* x)
{
    // http://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting
    const float A = 0.15f;
    const float B = 0.5f;
    const float C = 0.1f;
    const float D = 0.2f;
    const float E = 0.02f;
    const float F = 0.3f;

    TerraFloat3 ret;
    ret.x = ((x->x * (A * x->x + C * B) + D * E) / (x->x * (A * x->x + B) + D * F)) - E / F;
    ret.y = ((x->y * (A * x->y + C * B) + D * E) / (x->y * (A * x->y + B) + D * F)) - E / F;
    ret.z = ((x->z * (A * x->z + C * B) + D * E) / (x->z * (A * x->z + B) + D * F)) - E / F;
    return ret;
}

//--------------------------------------------------------------------------------------------------
TerraStats terra_render(const TerraCamera *camera, TerraScene *scene, const TerraFramebuffer *framebuffer, int x, int y, int width, int height)
{
    TerraStats stats;
    stats.total_ms = 0.f;
    stats.trace_total_ms = 0.f;
    stats.trace_min_ms = FLT_MAX;
    stats.trace_max_ms = -FLT_MAX;
    stats.trace_count = 0;

    TerraTimeSlice total_start = terra_timer_split();

    TerraFloat3 zaxis = terra_normf3(&camera->direction);
    TerraFloat3 xaxis = terra_crossf3(&camera->up, &zaxis);
    xaxis = terra_normf3(&xaxis);
    TerraFloat3 yaxis = terra_crossf3(&zaxis, &xaxis);

    TerraFloat4x4 rot;
    rot.rows[0] = terra_f4(xaxis.x, yaxis.x, zaxis.x, 0.f);
    rot.rows[1] = terra_f4(xaxis.y, yaxis.y, zaxis.y, 0.f);
    rot.rows[2] = terra_f4(xaxis.z, yaxis.z, zaxis.z, 0.f);
    rot.rows[3] = terra_f4(0.f, 0.f, 0.f, 1.f);
    for (int i = y; i < y + height; ++i)
    {
        for (int j = x; j < x + width; ++j)
        {
            const int spp = scene->opts.samples_per_pixel;
            TerraFloat3 acc = terra_f3_zero;
            
            for (int s = 0; s < spp; ++s)
            {
                TerraRay ray = terra_camera_ray(camera, framebuffer, j, i, scene->opts.subpixel_jitter, &rot);

                TerraTimeSlice trace_start = terra_timer_split();
                TerraFloat3 cur = terra_trace(scene, &ray);
                TerraTimeSlice trace_end = terra_timer_split();

                float trace_elapsed = (float)terra_timer_elapsed_ms(trace_end - trace_start);
                stats.trace_min_ms = (double)terra_minf((float)stats.trace_min_ms, trace_elapsed);
                stats.trace_max_ms = (double)terra_maxf((float)stats.trace_max_ms, trace_elapsed);
                stats.trace_total_ms += trace_elapsed;
                ++stats.trace_count;

                // Adding contribution
                acc = terra_addf3(&acc, &cur);
            }

            // Adding any previous partial result
            TerraRawIntegrationResult* partial = &framebuffer->results[i * framebuffer->width + j];
            partial->acc = terra_addf3(&acc, &partial->acc);
            partial->samples += spp;

            // Calculating final radiance
            TerraFloat3 color = terra_divf3(&partial->acc, (float)partial->samples);
            
            // Manual exposure
            color = terra_mulf3(&color, scene->opts.manual_exposure);
            switch (scene->opts.tonemapping_operator)
            {
                // TODO: Should exposure be 2^exposure as with f-stops ?
                // Gamma correction
            case kTerraTonemappingOperatorLinear:
            {
                color = terra_powf3(&color, 1.f / scene->opts.gamma);
                break;
            }
            // Simple version, local operator w/o white balancing
            case kTerraTonemappingOperatorReinhard:
            {
                // TODO: same as inv_dir invf3
                color.x = color.x / (1.f + color.x);
                color.y = color.y / (1.f + color.y);
                color.z = color.z / (1.f + color.z);
                color = terra_powf3(&color, 1.f / scene->opts.gamma);
                break;
            }
            // Approx
            case kTerraTonemappingOperatorFilmic:
            {
                // TODO: Better code pls
                TerraFloat3 x;
                x.x = terra_maxf(0.f, color.x - 0.004f);
                x.y = terra_maxf(0.f, color.y - 0.004f);
                x.z = terra_maxf(0.f, color.z - 0.004f);

                color.x = (x.x * (6.2f * x.x + 0.5f)) / (x.x * (6.2f * x.x + 1.7f) + 0.06f);
                color.y = (x.y * (6.2f * x.y + 0.5f)) / (x.y * (6.2f * x.y + 1.7f) + 0.06f);
                color.x = (x.z * (6.2f * x.z + 0.5f)) / (x.z * (6.2f * x.z + 1.7f) + 0.06f);

                // Gamma 2.2 included

                break;
            }
            case kTerraTonemappingOperatorUncharted2:
            {
                // TODO: Should white be tweaked ?
                // This is the white point in linear space
                const TerraFloat3 linear_white = terra_f3_set1(11.2f);

                TerraFloat3 white_scale = terra_tonemapping_uncharted2(&linear_white);
                white_scale.x = 1.f / white_scale.x;
                white_scale.y = 1.f / white_scale.y;
                white_scale.z = 1.f / white_scale.z;

                const float exposure_bias = 2.f;
                TerraFloat3 t = terra_mulf3(&color, exposure_bias);
                t = terra_tonemapping_uncharted2(&t);

                color = terra_pointf3(&t, &white_scale);

                color = terra_powf3(&color, 1.f / scene->opts.gamma);
                break;
            }
            default:
                break;
            }

            framebuffer->pixels[i * framebuffer->width + j] = color;
        }
    }

    TerraTimeSlice total_end = terra_timer_split();
    stats.total_ms = terra_timer_elapsed_ms(total_end - total_start);

    return stats;
}

TerraRay terra_camera_ray(const TerraCamera* camera, const TerraFramebuffer* framebuffer, int x, int y, float jitter, const TerraFloat4x4* rot_opt)
{
    TerraFloat3 dir = terra_get_pixel_pos(camera, framebuffer, x, y, jitter);
    if (rot_opt == NULL)
    {
        TerraFloat3 zaxis = terra_normf3(&camera->direction);
        TerraFloat3 xaxis = terra_crossf3(&camera->up, &zaxis);
        xaxis = terra_normf3(&xaxis);
        TerraFloat3 yaxis = terra_crossf3(&zaxis, &xaxis);

        TerraFloat4x4 rot;
        rot.rows[0] = terra_f4(xaxis.x, yaxis.x, zaxis.x, 0.f);
        rot.rows[1] = terra_f4(xaxis.y, yaxis.y, zaxis.y, 0.f);
        rot.rows[2] = terra_f4(xaxis.z, yaxis.z, zaxis.z, 0.f);
        rot.rows[3] = terra_f4(0.f, 0.f, 0.f, 1.f);
        dir = terra_transformf3(&rot, &dir);
    }
    else
        dir = terra_transformf3(rot_opt, &dir);

    TerraRay ray;
    ray.origin = camera->position;
    ray.direction = terra_normf3(&dir);
    ray.inv_direction.x = 1.f / ray.direction.x;
    ray.inv_direction.y = 1.f / ray.direction.y;
    ray.inv_direction.z = 1.f / ray.direction.z;

    return ray;
}

//--------------------------------------------------------------------------------------------------

TerraFloat3 terra_read_texture(const TerraTexture* texture, int x, int y)
{
    switch (texture->address_mode)
    {
    case kTerraTextureAddressClamp:
        x = (int) terra_minf((float) x, (float) (texture->width - 1));
        y = (int) terra_minf((float) y, (float) (texture->height - 1));
        break;
    case kTerraTextureAddressWrap:
        x = x % texture->width;
        y = y % texture->height;
        break;
        // TODO: Faster implementation
    case kTerraTextureAddressMirror:
        if ((int) (x / texture->width) % 2 == 0)
        {
            x = x % texture->width;
            y = y % texture->height;
        } else
        {
            x = texture->width - (x % texture->width);
            y = texture->height - (y % texture->height);
        }
        break;
    }

    uint8_t* pixel = &texture->pixels[(y * texture->width + x) * texture->comps + texture->offset];
    return terra_f3_set((float) pixel[0] / 255, (float) pixel[1] / 255, (float) pixel[2] / 255);
}

TerraFloat3 terra_read_hdr_texture(const TerraHDRTexture* texture, int x, int y)
{
    // HDR textures are RGB
    float* pixel = &texture->pixels[(y * texture->width + x) * 3];
    return *(TerraFloat3*) pixel;
}

TerraFloat3 terra_sample_texture(const TerraTexture* texture, const TerraFloat2* uv)
{
    // https://en.wikipedia.org/wiki/Bilinear_filtering
    const TerraFloat2 mapped_uv = terra_f2_set(uv->x * texture->width - 0.5f, uv->y * texture->height - 0.5f);

    const int ix = (int) mapped_uv.x;
    const int iy = (int) mapped_uv.y;

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

    const int ix = (int) mapped_uv.x;
    const int iy = (int) mapped_uv.y;

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

//--------------------------------------------------------------------------------------------------
#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
TerraTimeSlice terra_timer_split()
{
    LARGE_INTEGER ts;
    QueryPerformanceCounter(&ts);
    return (TerraTimeSlice)ts.QuadPart;
}

//--------------------------------------------------------------------------------------------------
double terra_timer_elapsed_ms(TerraTimeSlice delta)
{
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    return (double)delta / ((double)fq.QuadPart / 1000.f);
}
#else
#include <time.h>
TerraTimeSlice terra_timer_split()
{
    return (TerraTimeSlice)clock();
}

//--------------------------------------------------------------------------------------------------
double terra_timer_elapsed_ms(TerraTimeSlice delta)
{
    return (double)delta / CLOCKS_PER_SEC * 1000;
}
#endif


void* terra_malloc(size_t size)
{
    return malloc(size);
}

void terra_free(void* ptr)
{
    free(ptr);
}