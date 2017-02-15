#ifndef _TERRA_H_
#define _TERRA_H_

// Include
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <float.h>
#include <assert.h>
#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Math / Basic Types
//--------------------------------------------------------------------------------------------------
const float terra_PI = 3.1416926535f;
const float terra_PI2 = 6.283185307f;
const float terra_Epsilon = 0.0001f;

// 2D vector
typedef struct TerraFloat2
{
    float x, y;
} TerraFloat2;

// 3D vector
typedef struct TerraFloat3
{
    float x, y, z;
} TerraFloat3;

// 4D vector
typedef struct TerraFloat4
{
    float x, y, z, w;
} TerraFloat4;

// Matrix
typedef struct TerraFloat4x4
{
    TerraFloat4 rows[4];
} TerraFloat4x4;

// Ray
typedef struct TerraRay
{
    TerraFloat3 origin;
    TerraFloat3 direction;
    TerraFloat3 inv_direction;
} TerraRay;

#define terra_ior_air 1.f
#define terra_f3_zero terra_f3_set1(0.f)
#define terra_f3_one terra_f3_set1(1.f)

//--------------------------------------------------------------------------------------------------
// Shading Types
//--------------------------------------------------------------------------------------------------
// Context
typedef struct TerraShadingContext
{
    TerraFloat2   texcoord;
    TerraFloat3   normal;
    TerraFloat3   view; // w_o
    TerraFloat4x4 rot;
}TerraShadingContext;

// Some state shared inside a BSDF.
typedef struct TerraShadingState
{
    TerraFloat3 half_vector;
    TerraFloat3 albedo;
    float       roughness;
    float       metalness;
    float       emissive;
    float       fresnel;
}TerraShadingState;

struct  TerraMaterial;
typedef TerraFloat3(TerraRoutineSample)(const struct TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3);
typedef float       (TerraRoutineWeight)(const struct TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx);
typedef TerraFloat3(TerraRoutineShade) (const struct TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx);

typedef enum TerraBSDFType {
    kTerraBSDFDiffuse,
    kTerraBSDFRoughDielectric,
    kTerraBSDFGlass
} TerraBSDFType;

typedef struct TerraBSDF
{
    TerraRoutineSample* sample;
    TerraRoutineWeight* weight;
    TerraRoutineShade*  shade;
    TerraBSDFType type;
}TerraBSDF;

// All TerraTextures are LDR by default. Each component is one byte only.
// <comps> indicates how many components there are in the texture.
// <offset> indicates at what component we should start to read from.
// This are here in case a single texture is shared by multiple materials and each channel
// is used for a different component.
// A texture is invalid if pixels is NULL
typedef struct TerraTexture
{
    char*    name;
    uint8_t* pixels;
    uint16_t width;
    uint16_t height;
    uint8_t  comps;
    uint8_t  offset;
    uint8_t  filter;
    uint8_t  address_mode;
    bool     srgb;
}TerraTexture;

// HDR textures have 32-bit float components. 'half's are not accepted and should be converted before
// filling the data in. There is no comps/offset/filtering/addressing as HDRTexture is currently
// used only as cubemap and defaults to Bilinear. The only supported format is latitude/longitude
typedef struct TerraHDRTexture
{
    float*   pixels;
    uint16_t width;
    uint16_t height;
}TerraHDRTexture;

// Sampling filter to be applied. Mipmapping is not supported, thus no trilinear/anisotropic yet
typedef enum TerraFilter
{
    kTerraFilterPoint,
    kTerraFilterBilinear,
}TerraFilter;

// How to handle out of bound texture coordinates
typedef enum TerraTextureAddressMode
{
    kTerraTextureAddressWrap,
    kTerraTextureAddressMirror,
    kTerraTextureAddressClamp
}TerraTextureAddressMode;

typedef struct TerraAttribute
{
    TerraFloat3  value;
    TerraTexture map;
}TerraAttribute;

typedef struct TerraMaterial
{
    TerraBSDF bsdf;

    TerraAttribute albedo;
    TerraAttribute roughness;
    TerraAttribute metalness;
    TerraAttribute emissive;
    float ior;
} TerraMaterial;

//--------------------------------------------------------------------------------------------------
// Geometric types ( Scene )
//--------------------------------------------------------------------------------------------------
typedef struct TerraTriangle
{
    TerraFloat3 a;
    TerraFloat3 b;
    TerraFloat3 c;
    TerraFloat3 na;
    TerraFloat2 ta;
    TerraFloat3 nb;
    TerraFloat2 tb;
    TerraFloat3 nc;
    TerraFloat2 tc;
} TerraTriangle;

typedef int MaterialID;

typedef struct TerraObject
{
    TerraTriangle* triangles;
    int            triangles_count;
    TerraMaterial  material;
}TerraObject;

typedef enum TerraTonemappingOperator
{
    kTonemappingOperatorNone,
    kTonemappingOperatorLinear,
    kTonemappingOperatorReinhard,
    kTonemappingOperatorFilmic,
    kTonemappingOperatorUncharted2
}TerraTonemappingOperator;

typedef struct TerraSceneOptions
{
    // lat/long format
    TerraHDRTexture environment_map;
    TerraTonemappingOperator tonemapping_operator;

    bool  enable_direct_light_sampling;
    float subpixel_jitter;
    int   samples_per_pixel;
    int   bounces;

    float manual_exposure;
    float gamma;
}TerraSceneOptions;

typedef struct TerraAABB
{
    TerraFloat3 min;
    TerraFloat3 max;
} TerraAABB;

typedef struct TerraBVHNode
{
    TerraAABB aabb[2];
    int index[2];
} TerraBVHNode;

/*
typedef struct TerraSceneObject
{
    unsigned int index;
    int type;
} TerraSceneObject;
*/

typedef struct TerraBVHVolume
{
    TerraAABB aabb;
    unsigned int index;
    int type;
} TerraBVHVolume;

typedef struct TerraBVH
{
    TerraBVHNode* nodes;
    int           nodes_count;
    volatile int  traverse_stack_top;
    int*          traverse_stack[32];
} TerraBVH;

typedef struct TerraLight 
{
    TerraFloat3 center;
    float radius;
    TerraAABB aabb;
    float emissive;
} TerraLight;

typedef struct TerraScene
{
    TerraSceneOptions opts;

    TerraObject* objects;
    int          objects_count;

    TerraLight* lights;
    int lights_count;

    TerraBVH bvh;
} TerraScene;

typedef struct TerraCamera
{
    TerraFloat3 position;
    TerraFloat3 direction;
    TerraFloat3 up;
    float       fov;
} TerraCamera;

typedef struct TerraRawIntegrationResult
{
    TerraFloat3 acc;
    int         samples;
}TerraRawIntegrationResult;

typedef struct TerraStats
{
    // Total time elapsed in terra_render
    double total_ms;

    // Total time spent pathtracing
    double trace_total_ms;

    // Fastest ray
    double trace_min_ms;

    // Slowest ray
    double trace_max_ms;

    // Number of rays traced
    int trace_count;
}TerraStats;

typedef struct TerraFramebuffer
{
    TerraFloat3*               pixels;
    TerraRawIntegrationResult* results;
    int width;
    int height;
} TerraFramebuffer;

//--------------------------------------------------------------------------------------------------
// Terra public API
//--------------------------------------------------------------------------------------------------
void         terra_scene_begin(TerraScene* scene, int objects_count, int materials_count);
TerraObject* terra_scene_add_object(TerraScene* scene);
void         terra_scene_end(TerraScene* scene);
void         terra_scene_destroy(TerraScene* scene);

bool terra_framebuffer_create(TerraFramebuffer* framebuffer, int width, int height);
void terra_framebuffer_destroy(TerraFramebuffer* framebuffer);

TerraStats terra_render(const TerraCamera *camera, TerraScene *scene, const TerraFramebuffer *framebuffer, int x, int y, int width, int height);
TerraRay terra_camera_ray(const TerraCamera* camera, const TerraFramebuffer* framebuffer, int x, int y, float jitter, const TerraFloat4x4* rot_opt);

// Builtin BSDFs
void terra_bsdf_init(TerraBSDF* bsdf, TerraBSDFType type);
void terra_bsdf_init_diffuse(TerraBSDF* bsdf);
void terra_bsdf_init_rough_dielectric(TerraBSDF* bsdf);
void terra_bsdf_init_glass(TerraBSDF* bsdf);

//--------------------------------------------------------------------------------------------------
// Math functions
//--------------------------------------------------------------------------------------------------
TerraFloat2 terra_f2_set(float x, float y);
TerraFloat3 terra_f3_set(float x, float y, float z);
TerraFloat3 terra_f3_set1(float xyz);
TerraFloat4 terra_f4(float x, float y, float z, float w);
bool        terra_equalf3(TerraFloat3* a, TerraFloat3* b);
TerraFloat3 terra_addf3(const TerraFloat3 *left, const TerraFloat3 *right);
TerraFloat2 terra_addf2(const TerraFloat2* left, const TerraFloat2* right);
TerraFloat3 terra_subf3(const TerraFloat3 *left, const TerraFloat3 *right);
TerraFloat2 terra_mulf2(const TerraFloat2* left, float scale);
TerraFloat3 terra_mulf3(const TerraFloat3 *vec, float scale);
TerraFloat3 terra_powf3(const TerraFloat3* vec, float exp);
TerraFloat3 terra_pointf3(const TerraFloat3 * a, const TerraFloat3* b);
TerraFloat3 terra_divf3(const TerraFloat3 *vec, float val);
float       terra_dotf3(const TerraFloat3 *a, const TerraFloat3 *b);
TerraFloat3 terra_crossf3(const TerraFloat3 *a, const TerraFloat3 *b);
TerraFloat3 terra_negf3(const TerraFloat3 *vec);
float       terra_lenf3(const TerraFloat3 *vec);
float       terra_distance_squaredf3(const TerraFloat3 *a, const TerraFloat3 *b);
TerraFloat3 terra_normf3(const TerraFloat3 *vec);
float       terra_maxf(float a, float b);
float       terra_minf(float a, float b);
void        terra_swapf(float *a, float *b);
TerraFloat3 terra_transformf3(const TerraFloat4x4* transform, const TerraFloat3* vec);

//--------------------------------------------------------------------------------------------------
// Terra Internal routines
//--------------------------------------------------------------------------------------------------
TerraFloat3     terra_trace(TerraScene* scene, const TerraRay* ray);
TerraFloat3     terra_get_pixel_pos(const TerraCamera *camera, const TerraFramebuffer *frame, int x, int y, float half_range);
bool            terra_ray_triangle_intersection(const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out);
bool            terra_ray_aabb_intersection(const TerraRay *ray, const TerraAABB *aabb);
void            terra_triangle_init_shading(const TerraTriangle* triangle, const TerraFloat3* point, TerraShadingContext* ctx_out);
void            terra_aabb_fit_triangle(TerraAABB* aabb, const TerraTriangle* triangle);
TerraFloat3     terra_sample_texture(const TerraTexture* texture, const TerraFloat2* uv);
TerraFloat3     terra_sample_hdr_cubemap(const TerraHDRTexture* texture, const TerraFloat3* v);
TerraFloat3     terra_eval_attribute(const TerraAttribute* attribute, const TerraFloat2* uv);

typedef int64_t TerraTimeSlice;
int64_t terra_timer_split();
double   terra_timer_elapsed_ms(int64_t delta);

//--------------------------------------------------------------------------------------------------
// Terra BVH Internal routines
//--------------------------------------------------------------------------------------------------
void                terra_bvh_create(TerraBVH* bvh, const struct TerraScene* scene);
void                terra_bvh_destroy(TerraBVH* bvh);
TerraSceneObject    terra_bvh_traverse(TerraBVH* bvh, const TerraRay* ray, const struct TerraScene* scene, TerraFloat3* point);
float               terra_aabb_surface_area(const TerraAABB* aabb);
TerraFloat3         terra_aabb_center(const TerraAABB* aabb);
int                 terra_bvh_volume_compare_x(const void* left, const void* right);
int                 terra_bvh_volume_compare_y(const void* left, const void* right);
int                 terra_bvh_volume_compare_z(const void* left, const void* right);
int                 terra_bvh_sah_split_volumes(TerraBVHVolume* volumes, int volumes_count, const TerraAABB* container);

//--------------------------------------------------------------------------------------------------
// Terra implementation
//--------------------------------------------------------------------------------------------------
#if defined(TERRA_IMPLEMENTATION)

void* terra_malloc_impl(int size)
{
    return malloc(size);
}

void terra_free_impl(void* ptr)
{
    free(ptr);
}

#if defined(TERRA_MALLOC)
#   define terra_malloc terra_malloc_impl
#   define terra_free terra_free_impl
#else
#   if !(defined(terra_malloc))
#       error Please implement allocation routine or define TERRA_MALLOC
#   endif
#endif // TERRA_MALLOC

//--------------------------------------------------------------------------------------------------
void terra_scene_begin(TerraScene* scene, int objects_count, int materials_count)
{
    scene->objects = (TerraObject*)terra_malloc(sizeof(TerraObject) * objects_count);
    scene->objects_count = 0;
    scene->lights = (TerraLight*)terra_malloc(sizeof(TerraLight) * objects_count);
    scene->lights_count = 0;
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
    // Move objects

    terra_bvh_create(&scene->bvh, scene);
    for (int i = 0; i < 32; ++i)
        scene->bvh.traverse_stack[i] = NULL;
    scene->bvh.traverse_stack_top = 0;

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
    if (scene != NULL)
        terra_free(scene->objects);

    for (int i = 0; i < scene->bvh.traverse_stack_top; ++i)
        terra_free(scene->bvh.traverse_stack[i]);
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
    TerraFloat3 offset = terra_mulf3(normal, 0.0001 * sign);

    ray->origin = terra_addf3(point, &offset);
    ray->direction = *direction;

    ray->inv_direction.x = 1.f / ray->direction.x;
    ray->inv_direction.y = 1.f / ray->direction.y;
    ray->inv_direction.z = 1.f / ray->direction.z;
}

bool terra_find_closest(TerraScene* scene, const TerraRay* ray, const TerraMaterial** material_out, TerraShadingContext* ctx_out, TerraFloat3* intersection_point)
{
    TerraSceneObject obj = terra_bvh_traverse(&scene->bvh, ray, scene, intersection_point);
    int closest = obj.type;
    void* nearest_object = &scene->objects[obj.index & 0xff];
    int nearest_geometry = obj.index >> 8;

    // Nothing hit, returning background color
    if (closest == 0)
        return false;

    if (closest == 1)
    {
        const TerraObject* model = (TerraObject*)nearest_object;
        *material_out = &model->material;
        terra_triangle_init_shading(&model->triangles[nearest_geometry],
            intersection_point, ctx_out);
    }

    return true;
}

float terra_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

TerraFloat3 terra_lerpf3(const TerraFloat3* a, const TerraFloat3* b, float t)
{
    float x = terra_lerp(a->x, b->x, t);
    float y = terra_lerp(a->y, b->y, t);
    float z = terra_lerp(a->z, b->z, t);
    return terra_f3_set(x, y, z);
}

float terra_absf(float a)
{
    return a >= 0 ? a : -a;
}

TerraFloat3 terra_F_0(float ior, const TerraFloat3* albedo, float metalness)
{
    float f = terra_absf((1.f - ior) / (1.f + ior));
    f *= f;
    TerraFloat3 F0 = terra_f3_set1(f);
    F0 = terra_lerpf3(&F0, albedo, metalness);
    return F0;
}

TerraFloat3 terra_fresnel(const TerraFloat3* F_0, const TerraFloat3* view, const TerraFloat3* half_vector)
{
    float VoH = terra_maxf(0.f, terra_dotf3(view, half_vector));

    TerraFloat3 a = terra_f3_set(1 - F_0->x, 1 - F_0->y, 1 - F_0->z);
    a = terra_mulf3(&a, powf(1 - VoH, 5));
    return terra_addf3(&a, F_0);
}

TerraLight* terra_light_pick_power_proportional(const TerraScene* scene, float* e1) {
    // Compute total light emitted (in order to weight properly the lights when picking one)
    float total_light_power = 0;
    for (int i = 0; i < scene->lights_count; ++i) {
        total_light_power += scene->lights[i].emissive;
    }

    // Pick one light
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

    for (int bounce = 0; bounce < scene->opts.bounces; ++bounce)
    {
        const TerraMaterial* material;
        TerraShadingContext ctx;
        TerraFloat3 intersection_point;

        // Finding closest object
        if (terra_find_closest(scene, &ray, &material, &ctx, &intersection_point) == false)
        {
            // Nothing hit, returning scene radiance
            TerraFloat3 env_color = terra_sample_hdr_cubemap(&scene->opts.environment_map, &ray.direction);
            throughput = terra_pointf3(&throughput, &env_color);
            Lo = terra_addf3(&Lo, &throughput);
            break;
        }

        terra_build_rotation_around_normal(&ctx);
        ctx.view = terra_negf3(&ray.direction);

        TerraFloat3 mat_emissive = terra_eval_attribute(&material->emissive, &ctx.texcoord);
        TerraFloat3 mat_albedo = terra_eval_attribute(&material->albedo, &ctx.texcoord);

        // Adding emissive
        TerraFloat3 emissive = terra_mulf3(&throughput, mat_emissive.x);
        emissive = terra_pointf3(&emissive, &mat_albedo);
        Lo = terra_addf3(&Lo, &emissive);

        // Sampling BSDF
        float e0 = (float)rand() / RAND_MAX;
        float e1 = (float)rand() / RAND_MAX;
        float e2 = (float)rand() / RAND_MAX;

        TerraShadingState state;
        TerraFloat3 bsdf_sample = material->bsdf.sample(material, &state, &ctx, e0, e1, e2);
        float       bsdf_pdf = terra_maxf(material->bsdf.weight(material, &state, &bsdf_sample, &ctx), terra_Epsilon);

        float light_pdf = 0.f;
        if (scene->opts.enable_direct_light_sampling)
        {
            // Sampling light
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

        // BSDF Contribution
        TerraFloat3 bsdf_radiance = material->bsdf.shade(material, &state, &bsdf_sample, &ctx);
        float bsdf_weight = bsdf_pdf * bsdf_pdf / (light_pdf * light_pdf + bsdf_pdf * bsdf_pdf);
        TerraFloat3 bsdf_contribution = terra_mulf3(&bsdf_radiance, bsdf_weight / bsdf_pdf);

        throughput = terra_pointf3(&throughput, &bsdf_contribution);

        // Russian roulette
        /*float p = terra_maxf(throughput.x, terra_maxf(throughput.y, throughput.z));
        float e3 = 0.5f;//(float)rand() / RAND_MAX;
        if (e3 > p)
            break;
        //throughput = terra_pointf3(&throughput, &throughput);
        throughput = terra_divf3(&throughput, 0.5f);*/

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
    // TODO: Precalculate as many values as possible
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

                float trace_elapsed = terra_timer_elapsed_ms(trace_end - trace_start);
                stats.trace_min_ms = terra_minf(stats.trace_min_ms, trace_elapsed);
                stats.trace_max_ms = terra_maxf(stats.trace_max_ms, trace_elapsed);
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
            TerraFloat3 color = terra_divf3(&partial->acc, partial->samples);
            
            // Manual exposure
            color = terra_mulf3(&color, scene->opts.manual_exposure);
            switch (scene->opts.tonemapping_operator)
            {
                // TODO: Should exposure be 2^exposure as with f-stops ?
                // Gamma correction
            case kTonemappingOperatorLinear:
            {
                color = terra_powf3(&color, 1.f / scene->opts.gamma);
                break;
            }
            // Simple version, local operator w/o white balancing
            case kTonemappingOperatorReinhard:
            {
                // TODO: same as inv_dir invf3
                color.x = color.x / (1.f + color.x);
                color.y = color.y / (1.f + color.y);
                color.z = color.z / (1.f + color.z);
                color = terra_powf3(&color, 1.f / scene->opts.gamma);
                break;
            }
            // Approx
            case kTonemappingOperatorFilmic:
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
            case kTonemappingOperatorUncharted2:
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
// Preset: Diffuse (Lambertian)
//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_bsdf_diffuse_sample(const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3)
{
    float r = sqrtf(e1);
    float theta = 2 * terra_PI * e2;
    float x = r * cosf(theta);
    float z = r * sinf(theta);

    TerraFloat3 light = terra_f3_set(x, sqrtf(terra_maxf(0.f, 1 - e1)), z);
    return terra_transformf3(&ctx->rot, &light);
}

float terra_bsdf_diffuse_weight(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    return terra_dotf3(&ctx->normal, light) / terra_PI;
}

TerraFloat3 terra_bsdf_diffuse_shade(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    TerraFloat3 albedo = terra_eval_attribute(&material->albedo, &ctx->texcoord);
    float NoL = terra_maxf(0.f, terra_dotf3(&ctx->normal, light));
    return terra_mulf3(&albedo, NoL / terra_PI);
}

void terra_bsdf_init_diffuse(TerraBSDF* bsdf)
{
    bsdf->sample = terra_bsdf_diffuse_sample;
    bsdf->weight = terra_bsdf_diffuse_weight;
    bsdf->shade = terra_bsdf_diffuse_shade;
}

//--------------------------------------------------------------------------------------------------
// Preset: Rough-dielectric = Diffuse + Microfacet GGX specular
//--------------------------------------------------------------------------------------------------
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float terra_brdf_ctggx_chi(float val)
{
    return val <= 0.f ? 0.f : 1.f;
}

float terra_brdf_ctggx_G1(const TerraFloat3* v, const TerraFloat3* n, const TerraFloat3* h, float alpha2)
{
    float VoH = terra_dotf3(v, h);
    float VoN = terra_dotf3(v, n);

    float chi = terra_brdf_ctggx_chi(VoH / VoN);
    float VoH2 = VoH * VoH;
    float tan2 = (1.f - VoH2) / VoH2;
    return (chi * 2) / (sqrtf(1 + alpha2 * tan2) + 1);
}

float terra_brdf_ctggx_D(float NoH, float alpha2)
{
    float NoH2 = NoH * NoH;
    float den = NoH2 * alpha2 + (1 - NoH2);
    return (terra_brdf_ctggx_chi(NoH) * alpha2) / (terra_PI * den * den);
}

TerraFloat3 terra_bsdf_rough_dielectric_sample(const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3)
{
    state->roughness = terra_eval_attribute(&material->roughness, &ctx->texcoord).x;
    state->metalness = terra_eval_attribute(&material->metalness, &ctx->texcoord).x;

    const float pd = 1.f - state->metalness;
    const float ps = 1.f - pd;

    if (e3 <= pd)
    {
        TerraFloat3 light = terra_bsdf_diffuse_sample(material, NULL, ctx, e1, e2, 0);
        state->half_vector = terra_addf3(&light, &ctx->view);
        state->half_vector = terra_normf3(&state->half_vector);
        return light;
    }
    else
    {
        float alpha = state->roughness;

        // TODO: Can be optimized
        float theta = atanf((alpha * sqrtf(e1)) / sqrtf(1 - e1));
        float phi = 2 * terra_PI * e2;
        float sin_theta = sinf(theta);

        state->half_vector = terra_f3_set(sin_theta * cosf(phi), cosf(theta), sin_theta * sinf(phi));
        state->half_vector = terra_transformf3(&ctx->rot, &state->half_vector);
        state->half_vector = terra_normf3(&state->half_vector);

        float HoV = terra_maxf(0.f, terra_dotf3(&state->half_vector, &ctx->view));
        TerraFloat3 r = terra_mulf3(&state->half_vector, 2 * HoV);
        return terra_subf3(&r, &ctx->view);
    }
}

float terra_bsdf_rough_dielectric_weight(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    float alpha = state->roughness;
    float alpha2 = alpha * alpha;
    float NoH = terra_dotf3(&ctx->normal, &state->half_vector);

    float weight_specular = terra_brdf_ctggx_D(NoH, alpha2) * NoH;
    float weight_diffuse = terra_bsdf_diffuse_weight(material, NULL, light, ctx);

    const float pd = 1.f - state->metalness;
    const float ps = 1.f - pd;

    return weight_diffuse * pd + weight_specular * ps;
}

TerraFloat3 terra_bsdf_rough_dielectric_shade(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    TerraFloat3 albedo = terra_eval_attribute(&material->albedo, &ctx->texcoord);
    TerraFloat3 F_0 = terra_F_0(material->ior, &albedo, state->metalness);
    TerraFloat3 ks = terra_fresnel(&F_0, &ctx->view, &state->half_vector);

    // Specular
    float NoL = terra_maxf(terra_dotf3(&ctx->normal, light), 0.f);
    float NoV = terra_maxf(terra_dotf3(&ctx->normal, &ctx->view), 0.f);
    float NoH = terra_maxf(terra_dotf3(&ctx->normal, &state->half_vector), 0.f);

    float alpha = state->roughness;
    float alpha2 = alpha * alpha;

    // Fresnel (Schlick approx)
    float metalness = terra_eval_attribute(&material->metalness, &ctx->texcoord).x;

    // D
    float D = terra_brdf_ctggx_D(NoH, alpha2);

    // G
    float G = terra_brdf_ctggx_G1(&ctx->view, &ctx->normal, &state->half_vector, alpha2) *
        terra_brdf_ctggx_G1(light, &ctx->normal, &state->half_vector, alpha2);

    float den_CT = terra_minf((4 * NoL * NoV) + 0.05f, 1.f);

    TerraFloat3 specular_term = terra_mulf3(&ks, G * D / den_CT);

    // Diffuse
    TerraFloat3 diffuse_term = terra_bsdf_diffuse_shade(material, NULL, light, ctx);

    // Scaling contributes
    const float pd = 1.f - state->metalness;
    const float ps = 1.f - pd;
    TerraFloat3 diffuse_factor = terra_f3_set(1.f - ks.x, 1.f - ks.y, 1.f - ks.z);
    diffuse_factor = terra_mulf3(&diffuse_factor, (1.f - state->metalness) * pd);
    diffuse_term = terra_pointf3(&diffuse_term, &diffuse_factor);

    specular_term = terra_mulf3(&specular_term, ps);

    TerraFloat3 reflectance = terra_addf3(&diffuse_term, &specular_term);

    return terra_mulf3(&reflectance, NoL);
}

void terra_bsdf_init_rough_dielectric(TerraBSDF* bsdf)
{
    bsdf->sample = terra_bsdf_rough_dielectric_sample;
    bsdf->weight = terra_bsdf_rough_dielectric_weight;
    bsdf->shade = terra_bsdf_rough_dielectric_shade;
}

//--------------------------------------------------------------------------------------------------
// Preset: Perfect glass
//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_bsdf_glass_sample(const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3)
{
    TerraFloat3 normal = ctx->normal;
    TerraFloat3 incident = terra_negf3(&ctx->view);

    float n1, n2;
    float cos_i = terra_dotf3(&normal, &incident);

    // n1 to n2. Flipping in case we are inside a material
    if (cos_i > 0.f)
    {
        n1 = material->ior;
        n2 = terra_ior_air;
        normal = terra_negf3(&normal);
    }
    else
    {
        n1 = terra_ior_air;
        n2 = material->ior;
        cos_i = -cos_i;
    }

    // Reflection
    TerraFloat3 refl = terra_mulf3(&normal, 2 * terra_dotf3(&normal, &incident));
    refl = terra_subf3(&incident, &refl);

    // TIR ~ Faster version than asin(n2 / n1)
    float nni = (n1 / n2);
    float cos_t = 1.f - nni * nni * (1.f - cos_i * cos_i);
    if (cos_t < 0.f)
    {
        state->fresnel = 1.f;
        return refl;
    }

    cos_t = sqrtf(cos_t);

    // Unpolarized schlick fresnel
    float t = 1.f - (n1 <= n2 ? cos_i : cos_t);
    float R0 = (n1 - n2) / (n1 + n2);
    R0 *= R0;
    float R = R0 + (1 - R0) * (t*t*t*t*t);

    // Russian roulette reflection/transmission
    if (e3 < R)
    {
        state->fresnel = R;
        return refl;
    }

    // Transmitted direction
    // the '-' is because our view is the opposite of the incident direction
    TerraFloat3 trans_v = terra_mulf3(&normal, (n1 / n2) * cos_i - cos_t);
    TerraFloat3 trans_n = terra_mulf3(&incident, n1 / n2);
    TerraFloat3 trans = terra_addf3(&trans_v, &trans_n);
    trans = terra_normf3(&trans);
    state->fresnel = 1 - R;
    return trans;
}

float terra_bsdf_glass_weight(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    return state->fresnel;
}

TerraFloat3 terra_bsdf_glass_shade(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    TerraFloat3 albedo = terra_eval_attribute(&material->albedo, &ctx->texcoord);
    return terra_mulf3(&albedo, state->fresnel);
}

void terra_bsdf_init_glass(TerraBSDF* bsdf)
{
    bsdf->sample = terra_bsdf_glass_sample;
    bsdf->weight = terra_bsdf_glass_weight;
    bsdf->shade = terra_bsdf_glass_shade;
}

// BSDF INIT
void terra_bsdf_init(TerraBSDF* bsdf, TerraBSDFType type)
{
    bsdf->type = type;
    switch (type) {
    case kTerraBSDFDiffuse:
        bsdf->sample = terra_bsdf_diffuse_sample;
        bsdf->weight = terra_bsdf_diffuse_weight;
        bsdf->shade = terra_bsdf_diffuse_shade;
        break;
    case kTerraBSDFRoughDielectric:
        bsdf->sample = terra_bsdf_rough_dielectric_sample;
        bsdf->weight = terra_bsdf_rough_dielectric_weight;
        bsdf->shade = terra_bsdf_rough_dielectric_shade;
        break;
    case kTerraBSDFGlass:
        bsdf->sample = terra_bsdf_glass_sample;
        bsdf->weight = terra_bsdf_glass_weight;
        bsdf->shade = terra_bsdf_glass_shade;
    }
}

//--------------------------------------------------------------------------------------------------
bool terra_ray_triangle_intersection(const TerraRay* ray, const TerraTriangle* triangle, TerraFloat3* point_out)
{
    const TerraTriangle *tri = triangle;

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
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
bool terra_ray_aabb_intersection(const TerraRay *ray, const TerraAABB *aabb)
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

    return tmax > terra_maxf(tmin, 0.f);
}

//--------------------------------------------------------------------------------------------------
void terra_triangle_init_shading(const TerraTriangle* triangle, const TerraFloat3* point, TerraShadingContext* ctx)
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

    TerraFloat3 na = terra_mulf3(&triangle->nc, uv.y);
    TerraFloat3 nb = terra_mulf3(&triangle->nb, uv.x);
    TerraFloat3 nc = terra_mulf3(&triangle->na, 1 - uv.x - uv.y);

    ctx->normal = terra_addf3(&na, &nb);
    ctx->normal = terra_addf3(&ctx->normal, &nc);
    ctx->normal = terra_normf3(&ctx->normal);

    TerraFloat2 ta = terra_mulf2(&triangle->tc, uv.y);
    TerraFloat2 tb = terra_mulf2(&triangle->tb, uv.x);
    TerraFloat2 tc = terra_mulf2(&triangle->ta, 1 - uv.x - uv.y);

    ctx->texcoord = terra_addf2(&ta, &tb);
    ctx->texcoord = terra_addf2(&ctx->texcoord, &tc);
    ctx->texcoord = ctx->texcoord;
}

//--------------------------------------------------------------------------------------------------
// Terra BVH Internal routines implementation
//--------------------------------------------------------------------------------------------------
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

    aabb->max.x = terra_maxf(aabb->max.x, triangle->a.x);
    aabb->max.x = terra_maxf(aabb->max.x, triangle->b.x);
    aabb->max.x = terra_maxf(aabb->max.x, triangle->c.x);
    aabb->max.y = terra_maxf(aabb->max.y, triangle->a.y);
    aabb->max.y = terra_maxf(aabb->max.y, triangle->b.y);
    aabb->max.y = terra_maxf(aabb->max.y, triangle->c.y);
    aabb->max.z = terra_maxf(aabb->max.z, triangle->a.z);
    aabb->max.z = terra_maxf(aabb->max.z, triangle->b.z);
    aabb->max.z = terra_maxf(aabb->max.z, triangle->c.z);
}

TerraFloat3 terra_read_texture(const TerraTexture* texture, int x, int y)
{
    switch (texture->address_mode)
    {
    case kTerraTextureAddressClamp:
        x = terra_minf(x, texture->width - 1);
        y = terra_minf(y, texture->height - 1);
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
        int x2 = min(ix + 1, texture->width - 1);
        int y2 = iy;

        // BL
        int x3 = ix;
        int y3 = min(iy + 1, texture->height - 1);

        // BR
        int x4 = min(ix + 1, texture->width - 1);
        int y4 = min(iy + 1, texture->height - 1);

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

    int x2 = min(ix + 1, texture->width - 1);
    int y2 = iy;

    int x3 = ix;
    int y3 = min(iy + 1, texture->height - 1);

    int x4 = min(ix + 1, texture->width - 1);
    int y4 = min(iy + 1, texture->height - 1);

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
    float* left_area = (float*)malloc(sizeof(*left_area) * (volumes_count - 1));
    float* right_area = (float*)malloc(sizeof(*right_area) * (volumes_count - 1));
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

    TerraBVHVolume* volumes = (TerraBVHVolume*)malloc(sizeof(*volumes) * volumes_count);
    for (int i = 0; i < volumes_count; ++i)
    {
        volumes[i].aabb.min = terra_f3_set1(FLT_MAX);
        volumes[i].aabb.max = terra_f3_set1(-FLT_MAX);
    }
    int p = 0;

    // build the scene aabb and the volumes aabb
    for (unsigned int j = 0; j < scene->objects_count; ++j)
    {
        for (unsigned int i = 0; i < scene->objects[j].triangles_count; ++i, ++p)
        {
            terra_aabb_fit_triangle(&scene_aabb, &scene->objects[j].triangles[i]);
            terra_aabb_fit_triangle(&volumes[p].aabb, &scene->objects[j].triangles[i]);
            volumes[p].type = 1;
            volumes[p].index = j | (i << 8);
        }
    }

    // build the bvh. we do iterative building using a stack
    bvh->nodes = (TerraBVHNode*)malloc(sizeof(*bvh->nodes) * volumes_count * 2);
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
    free(bvh->nodes);
}

TerraSceneObject terra_bvh_traverse(TerraBVH* bvh, const TerraRay* ray, const TerraScene* scene, TerraFloat3* point)
{
    static __thread int* queue = NULL;
    if (queue == NULL)
    {
        queue = (int*)terra_malloc(sizeof(*queue) * bvh->nodes_count);

        // registering for deallocation
        int top = (int)InterlockedAdd((volatile LONG*)&scene->bvh.traverse_stack_top, 1);
        bvh->traverse_stack[top] = queue;
    }
    //int* queue = malloc(sizeof(int) * 2 * bvh->nodes_count);
    queue[0] = 0;
    int queue_count = 1;

    int node = 0;
    int type = 0;
    int child = -1;

    TerraSceneObject obj;
    obj.type = 0;
    float min_d = FLT_MAX;
    TerraFloat3 min_p = terra_f3_set1(FLT_MAX);

    while (queue_count > 0)
    {
        node = queue[--queue_count];
        for (int i = 0; i < 2; ++i)
        {
            switch (bvh->nodes[node].type[i])
            {
            case -1:
                // not leaf
                if (terra_ray_aabb_intersection(ray, &bvh->nodes[node].aabb[i]))
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
                if (terra_ray_triangle_intersection(ray, &scene->objects[model_idx].triangles[tri_idx], &p))
                {
                    TerraFloat3 po = terra_subf3(&p, &ray->origin);
                    float d = terra_lenf3(&po);
                    if (d < min_d)
                    {
                        min_d = d;
                        min_p = p;
                        obj.index = bvh->nodes[node].index[i];
                        obj.type = bvh->nodes[node].type[i];
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
    //free(queue);
    *point = min_p;
    return obj;
}

//--------------------------------------------------------------------------------------------------
#if defined(TERRA_TIMING)
#include <Windows.h>
int64_t terra_timer_split()
{
    LARGE_INTEGER ts;
    QueryPerformanceCounter(&ts);
    return (int64_t)ts.QuadPart;
}

//--------------------------------------------------------------------------------------------------
double terra_timer_elapsed_ms(int64_t delta)
{
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    return (double)delta / ((double)fq.QuadPart / 1000.f);
}
#else
uint64_t terra_timer_split()
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
float terra_timer_elapsed_ms(uint64_t delta)
{
    return 0.f;
}
#endif

//--------------------------------------------------------------------------------------------------
// Math implementation
//--------------------------------------------------------------------------------------------------
#if !defined(TERRA_ENABLE_SIMD)
inline TerraFloat2 terra_f2_set(float x, float y)
{
    TerraFloat2 ret;
    ret.x = x;
    ret.y = y;
    return ret;
}

TerraFloat3 terra_f3_set(float x, float y, float z)
{
    TerraFloat3 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    return ret;
}

TerraFloat3 terra_f3_set1(float xyz)
{
    TerraFloat3 ret;
    ret.x = ret.y = ret.z = xyz;
    return ret;
}

inline TerraFloat4 terra_f4(float x, float y, float z, float w)
{
    TerraFloat4 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;
    return ret;
}

bool terra_equalf3(TerraFloat3* a, TerraFloat3* b) {
    return (a->x == b->x && a->y == b->y && a->z == b->z);
}

TerraFloat3 terra_addf3(const TerraFloat3 *left, const TerraFloat3 *right)
{
    return terra_f3_set(
        left->x + right->x,
        left->y + right->y,
        left->z + right->z);
}

inline TerraFloat2 terra_addf2(const TerraFloat2 * left, const TerraFloat2 * right)
{
    return terra_f2_set(left->x + right->x, left->y + right->y);
}

TerraFloat3 terra_subf3(const TerraFloat3 *left, const TerraFloat3 *right)
{
    return terra_f3_set(
        left->x - right->x,
        left->y - right->y,
        left->z - right->z);
}

inline TerraFloat2 terra_mulf2(const TerraFloat2 * left, float scale)
{
    return terra_f2_set(left->x * scale, left->y * scale);
}

TerraFloat3 terra_mulf3(const TerraFloat3 *vec, float scale)
{
    return terra_f3_set(
        vec->x * scale,
        vec->y * scale,
        vec->z * scale);
}

TerraFloat3 terra_powf3(const TerraFloat3* vec, float exp)
{
    return terra_f3_set(
        powf(vec->x, exp),
        powf(vec->y, exp),
        powf(vec->z, exp)
    );
}

TerraFloat3 terra_pointf3(const TerraFloat3 *a, const TerraFloat3* b)
{
    return terra_f3_set(a->x * b->x, a->y * b->y, a->z * b->z);
}

TerraFloat3 terra_divf3(const TerraFloat3 *vec, float val)
{
    return terra_f3_set(
        vec->x / val,
        vec->y / val,
        vec->z / val);
}

float terra_dotf3(const TerraFloat3 *a, const TerraFloat3 *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

TerraFloat3 terra_crossf3(const TerraFloat3 *a, const TerraFloat3 *b)
{
    return terra_f3_set(
        a->y * b->z - a->z * b->y,
        a->z * b->x - a->x * b->z,
        a->x * b->y - a->y * b->x);
}

TerraFloat3 terra_negf3(const TerraFloat3 *vec)
{
    return terra_f3_set(
        -vec->x,
        -vec->y,
        -vec->z);
}

float terra_lenf3(const TerraFloat3 *vec)
{
    return sqrtf(
        vec->x * vec->x +
        vec->y * vec->y +
        vec->z * vec->z);
}

float terra_distance_squaredf3(const TerraFloat3 *a, const TerraFloat3 *b)
{
    TerraFloat3 delta = terra_subf3(b, a);
    return terra_dotf3(&delta, &delta);
}

TerraFloat3 terra_normf3(const TerraFloat3 *vec)
{
    // Substitute with inverse len
    float len = terra_lenf3(vec);
    return terra_f3_set(
        vec->x / len,
        vec->y / len,
        vec->z / len);
}

float terra_maxf(float a, float b)
{
    return a > b ? a : b;
}

float terra_minf(float a, float b)
{
    return a < b ? a : b;
}

void terra_swapf(float *a, float *b)
{
    float t = *a;
    *a = *b;
    *b = t;
}

TerraFloat3 terra_transformf3(const TerraFloat4x4 * transform, const TerraFloat3 * vec)
{
    return terra_f3_set(terra_dotf3((TerraFloat3*)&transform->rows[0], vec),
        terra_dotf3((TerraFloat3*)&transform->rows[1], vec),
        terra_dotf3((TerraFloat3*)&transform->rows[2], vec));
}

bool terra_f3_is_zero(const TerraFloat3* f3)
{
    return f3->x == 0 && f3->y == 0 && f3->z == 0;
}

#else
TerraFloat3 terra_f3(float x, float y, float z)
{
    return (TerraFloat3) { x, y, z };
}

inline TerraFloat4 terra_f4(float x, float y, float z, float w)
{
    return (TerraFloat4) { x, y, z, w };
}

bool terra_equalf3(TerraFloat3* a, TerraFloat3* b) {
    return (a->x == b->x && a->y == b->y && a->z == b->z);
}

TerraFloat3 terra_addf3(const TerraFloat3 *left, const TerraFloat3 *right)
{
    return terra_f3(
        left->x + right->x,
        left->y + right->y,
        left->z + right->z);
}

TerraFloat3 terra_subf3(const TerraFloat3 *left, const TerraFloat3 *right)
{
    return terra_f3(
        left->x - right->x,
        left->y - right->y,
        left->z - right->z);
}

TerraFloat3 terra_mulf3(const TerraFloat3 *vec, float scale)
{
    return terra_f3(
        vec->x * scale,
        vec->y * scale,
        vec->z * scale);
}

TerraFloat3 terra_pointf3(const TerraFloat3 *a, const TerraFloat3* b)
{
    return terra_f3(a->x * b->x, a->y * b->y, a->z * b->z);
}

TerraFloat3 terra_divf3(const TerraFloat3 *vec, float val)
{
    return terra_f3(
        vec->x / val,
        vec->y / val,
        vec->z / val);
}

float terra_dotf3(const TerraFloat3 *a, const TerraFloat3 *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

TerraFloat3 terra_crossf3(const TerraFloat3 *a, const TerraFloat3 *b)
{
    return terra_f3(
        a->y * b->z - a->z * b->y,
        a->z * b->x - a->x * b->z,
        a->x * b->y - a->y * b->x);
}

TerraFloat3 terra_negf3(const TerraFloat3 *vec)
{
    return terra_f3(
        -vec->x,
        -vec->y,
        -vec->z);
}

float terra_lenf3(const TerraFloat3 *vec)
{
    return sqrtf(
        vec->x * vec->x +
        vec->y * vec->y +
        vec->z * vec->z);
}

float terra_distance_squaredf3(const TerraFloat3 *a, const TerraFloat3 *b)
{
    TerraFloat3 delta = terra_subf3(b, a);
    return terra_dotf3(&delta, &delta);
}

TerraFloat3 terra_normf3(const TerraFloat3 *vec)
{
    // Substitute with inverse len
    float len = terra_lenf3(vec);
    return terra_f3(
        vec->x / len,
        vec->y / len,
        vec->z / len);
}

float terra_maxf(float a, float b)
{
    return a > b ? a : b;
}

float terra_minf(float a, float b)
{
    return a < b ? a : b;
}

void terra_swapf(float *a, float *b)
{
    float t = *a;
    *a = *b;
    *b = t;
}

TerraFloat3 terra_transformf3(const TerraFloat4x4 * transform, const TerraFloat3 * vec)
{
    return terra_f3(terra_dotf3((TerraFloat3*)&transform->rows[0], vec),
        terra_dotf3((TerraFloat3*)&transform->rows[1], vec),
        terra_dotf3((TerraFloat3*)&transform->rows[2], vec));
}


#endif // TERRA_ENABLE_SIMD

#endif // TERRA_IMPLEMENTATION

#endif // _TERRA_H_