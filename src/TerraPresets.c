// Header
#include "TerraPresets.h"

TerraFloat3 terra_lerpf3(const TerraFloat3* a, const TerraFloat3* b, float t)
{
    float x = terra_lerp(a->x, b->x, t);
    float y = terra_lerp(a->y, b->y, t);
    float z = terra_lerp(a->z, b->z, t);
    return terra_f3_set(x, y, z);
}

TerraFloat3 terra_fresnel(const TerraFloat3* F_0, const TerraFloat3* view, const TerraFloat3* half_vector)
{
    float VoH = terra_maxf(0.f, terra_dotf3(view, half_vector));

    TerraFloat3 a = terra_f3_set(1 - F_0->x, 1 - F_0->y, 1 - F_0->z);
    a = terra_mulf3(&a, powf(1 - VoH, 5));
    return terra_addf3(&a, F_0);
}

TerraFloat3 terra_F_0(float ior, const TerraFloat3* albedo, float metalness)
{
    float f = (1.f - ior) / (1.f + ior);
    f *= f;
    TerraFloat3 F0 = terra_f3_set1(f);
    F0 = terra_lerpf3(&F0, albedo, metalness);
    return F0;
}

//--------------------------------------------------------------------------------------------------
// Preset: Diffuse (Lambertian)
//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_bsdf_diffuse_sample(const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo)
{
    float r = sqrtf(e1);
    float theta = 2 * terra_PI * e2;
    float x = r * cosf(theta);
    float z = r * sinf(theta);

    TerraFloat3 wi = terra_f3_set(x, sqrtf(terra_maxf(0.f, 1 - e1)), z);
    return terra_transformf3(&surface->rot, &wi);
}

float terra_bsdf_diffuse_pdf(const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo)
{
    return terra_dotf3(&surface->normal, wi) / terra_PI;
}

TerraFloat3 terra_bsdf_diffuse_eval(const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo)
{
    float NoL = terra_maxf(0.f, terra_dotf3(&surface->normal, wi));
    return terra_mulf3(&surface->attributes[TERRA_DIFFUSE_ALBEDO], NoL / terra_PI);
}

void terra_bsdf_init_diffuse(TerraBSDF* bsdf)
{
    bsdf->sample = terra_bsdf_diffuse_sample;
    bsdf->pdf = terra_bsdf_diffuse_pdf;
    bsdf->eval = terra_bsdf_diffuse_eval;
}

//--------------------------------------------------------------------------------------------------
// Preset: Actually just phong BRDF
//--------------------------------------------------------------------------------------------------
void terra_bsdf_phong__calculate_ks_kd(const TerraShadingSurface* surface, float* kd, float* ks)
{
    float diffuse_reflectivity = terra_maxf(surface->attributes[TERRA_PHONG_ALBEDO].x + 
                                            surface->attributes[TERRA_PHONG_ALBEDO].y + 
                                            surface->attributes[TERRA_PHONG_ALBEDO].z, 
                                            terra_Epsilon);
    float specular_reflectivity = surface->attributes[TERRA_PHONG_SPECULAR_COLOR].x + 
                                  surface->attributes[TERRA_PHONG_SPECULAR_COLOR].y + 
                                  surface->attributes[TERRA_PHONG_SPECULAR_COLOR].z;

    if (specular_reflectivity > diffuse_reflectivity)
    {
        *kd = 0.5 * diffuse_reflectivity / specular_reflectivity;
        *ks = 1.f - *kd;
    }
    else
    {
        *ks = 0.5 * specular_reflectivity / diffuse_reflectivity;
        *kd = 1.f - *ks;
    }
}

TerraFloat3 terra_bsdf_phong_sample(const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo)
{
    // In order to decide which ray to pick we need two values kd and ks with kd + ks <= 1 
    // but kd and ks are vector, but they still represent the amount of reflectivity. Thus 
    // we normalize them to [0, 1] keeping the ratios. If a surface has Ks = (0 0 0) There is
    // no point in casting specular rays.
    float kd, ks;
    terra_bsdf_phong__calculate_ks_kd(surface, &kd, &ks);

    if (e3 < kd)
    {
        float r = sqrtf(e1);
        float theta = 2 * terra_PI * e2;
        float x = r * cosf(theta);
        float z = r * sinf(theta);

        TerraFloat3 wi = terra_f3_set(x, sqrtf(terra_maxf(0.f, 1 - e1)), z);
        return terra_transformf3(&surface->rot, &wi);
    }
    else
    {
        float phi = terra_PI2 * e1;
        float theta = acosf(powf(1.f - e2, 1.f / (surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x + 1)));
        float sin_theta = sinf(theta);

        TerraFloat3 wi = terra_f3_set(sin_theta * cosf(phi), cosf(theta), sin_theta * sinf(phi));
        wi = terra_transformf3(&surface->rot, &wi);
        return terra_normf3(&wi);
    }
}

float terra_bsdf_phong_pdf(const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo)
{
    float alpha = acosf(terra_dotf3(wi, wo));
    return powf(cosf(alpha), surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x) * (surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x + 1) / terra_PI2;
}

TerraFloat3 terra_bsdf_phong_eval(const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo)
{
    TerraFloat3 diffuse_term = terra_mulf3(&surface->attributes[TERRA_PHONG_ALBEDO], 1.f / terra_PI);

    float alpha = terra_dotf3(wi, wo);
    float cos_n_alpha = powf(alpha, surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x);
    TerraFloat3 specular_term = terra_mulf3(&surface->attributes[TERRA_PHONG_SPECULAR_COLOR], (surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x + 2) / terra_PI2);
    specular_term = terra_mulf3(&specular_term, cos_n_alpha);

    // Weighting by pdf
    float kd, ks;
    terra_bsdf_phong__calculate_ks_kd(surface, &kd, &ks);
    diffuse_term = terra_mulf3(&diffuse_term, kd);
    specular_term = terra_mulf3(&specular_term, ks);
    return terra_addf3(&diffuse_term, &specular_term);
}

void terra_bsdf_init_phong(TerraBSDF* bsdf)
{
    bsdf->sample = terra_bsdf_phong_sample;
    bsdf->pdf = terra_bsdf_phong_pdf;
    bsdf->eval = terra_bsdf_phong_eval;
}

#if 0
//--------------------------------------------------------------------------------------------------
// Preset: Rough-dielectric = Diffuse + Microfacet GGX specular
//--------------------------------------------------------------------------------------------------
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float terra_brdf_ggx_chi(float val)
{
    return val <= 0.f ? 0.f : 1.f;
}

#if 1
float terra_brdf_ggx_G1(const TerraFloat3* v, const TerraFloat3* n, const TerraFloat3* h, float alpha2)
{
    float VoH = terra_dotf3(v, h);
    float VoN = terra_dotf3(v, n);

    float chi = terra_brdf_ggx_chi(VoH / VoN);
    float VoH2 = VoH * VoH;
    float tan2 = (1.f - VoH2) / VoH2;
    return (chi * 2) / (sqrtf(1 + alpha2 * tan2) + 1);
}

float terra_brdf_ggx_D(float NoH, float alpha2)
{
    float NoH2 = NoH * NoH;
    float den = NoH2 * alpha2 + (1 - NoH2);
    return (terra_brdf_ggx_chi(NoH) * alpha2) / (terra_PI * den * den);
}
TerraFloat3 terra_brdf_rough_dielectric_sample(const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3)
{
    state->roughness = terra_eval_attribute(&material->roughness, &ctx->texcoord).x;
    state->metalness = terra_eval_attribute(&material->metalness, &ctx->texcoord).x;

    const float pd = 1.f - state->metalness;

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

float terra_brdf_rough_dielectric_weight(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    float alpha = state->roughness;
    float alpha2 = alpha * alpha;
    float NoH = terra_dotf3(&ctx->normal, &state->half_vector);

    float weight_specular = terra_brdf_ggx_D(NoH, alpha2) * NoH;
    float weight_diffuse = terra_bsdf_diffuse_weight(material, NULL, light, ctx);

    const float pd = 1.f - state->metalness;
    const float ps = 1.f - pd;

    return weight_diffuse * pd + weight_specular * ps;
}

TerraFloat3 terra_brdf_rough_dielectric_shade(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
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
    // float metalness = terra_eval_attribute(&material->metalness, &ctx->texcoord).x;

    // D
    float D = terra_brdf_ggx_D(NoH, alpha2);

    // G
    float G = terra_brdf_ggx_G1(&ctx->view, &ctx->normal, &state->half_vector, alpha2) *
        terra_brdf_ggx_G1(light, &ctx->normal, &state->half_vector, alpha2);

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

#else
theta_m ~ angle between m and n
theta_v ~ angle between v and n
float terra_brdf_ggx_G1(const TerraFloat3* v, const TerraFloat3* n, const TerraFloat3* h, float alpha2)
{
    float VoH = terra_dotf3(v, h);
    float VoN = terra_dotf3(v, n);

    float left = terra_brdf_ggx_chi(VoH / VoN);
    float tan2 = tanf(acosf(VoN));
    tan2 *= tan2;
    return left * 2.f / (1 + sqrtf(1 + alpha2 * tan2));
}

float terra_brdf_ggx_D(float NoH, float alpha2)
{
    float NoH2 = NoH * NoH;
    float theta_m = acosf(NoH);
    float tan2_m = tanf(theta_m);
    tan2_m *= tan2_m;
    float den = terra_PI * NoH2 * NoH2 * ((tan2_m + alpha2) + (tan2_m + alpha2));
    return terra_brdf_ggx_chi(NoH) * alpha2 / den;
}

TerraFloat3 terra_brdf_rough_dielectric_sample(const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3)
{
    state->roughness = terra_eval_attribute(&material->roughness, &ctx->texcoord).x;

    // Sampling D
    float alpha = state->roughness;
    float theta = atanf(alpha * sqrtf(e1) / sqrtf(1.f - e1));
    float phi = terra_PI2 * e2;
    float sin_theta = sinf(theta);

    state->half_vector = terra_f3_set(sin_theta * cosf(phi), cosf(theta), sin_theta * sinf(phi));
    state->half_vector = terra_transformf3(&ctx->rot, &state->half_vector);
    state->half_vector = terra_normf3(&state->half_vector);

    // Reflecting view from sampled halfvector
    float HoV = terra_maxf(0.f, terra_dotf3(&state->half_vector, &ctx->view));
    TerraFloat3 r = terra_mulf3(&state->half_vector, 2 * HoV);
    return terra_subf3(&r, &ctx->view);
}

float terra_brdf_rough_dielectric_weight(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    float alpha = state->roughness;
    float alpha2 = alpha * alpha;
    float HoV = terra_dotf3(&ctx->view, &state->half_vector);
    float NoV = terra_dotf3(&ctx->view, &ctx->normal);
    float HoN = terra_dotf3(&state->half_vector, &ctx->normal);

    float G = terra_brdf_ggx_G1(&ctx->view, &ctx->normal, &state->half_vector, alpha2) *
        terra_brdf_ggx_G1(light, &ctx->normal, &state->half_vector, alpha2);

    return HoV * G / (NoV * HoN);
}

TerraFloat3 terra_brdf_rough_dielectric_shade(const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx)
{
    TerraFloat3 albedo = terra_eval_attribute(&material->albedo, &ctx->texcoord);
    TerraFloat3 F_0 = terra_F_0(material->ior, &albedo, state->metalness);
    TerraFloat3 F = terra_fresnel(&F_0, &ctx->view, &state->half_vector);

    // Specular
    float NoL = terra_maxf(terra_dotf3(&ctx->normal, light), 0.f);
    float NoV = terra_maxf(terra_dotf3(&ctx->normal, &ctx->view), 0.f);
    float NoH = terra_maxf(terra_dotf3(&ctx->normal, &state->half_vector), 0.f);

    float alpha = state->roughness;
    float alpha2 = alpha * alpha;

    // D
    float D = terra_brdf_ggx_D(NoH, alpha2);

    // G
    float G = terra_brdf_ggx_G1(&ctx->view, &ctx->normal, &state->half_vector, alpha2) *
        terra_brdf_ggx_G1(light, &ctx->normal, &state->half_vector, alpha2);

    float den = terra_minf((4 * NoL * NoV) + 0.05f, 1.f);

    TerraFloat3 num = terra_mulf3(&F, D);
    num = terra_mulf3(&num, G);
    return terra_mulf3(&num, 1.f / den);
}
#endif

void terra_bsdf_init_rough_dielectric(TerraBSDF* bsdf)
{
    bsdf->sample = terra_brdf_rough_dielectric_sample;
    bsdf->weight = terra_brdf_rough_dielectric_weight;
    bsdf->shade = terra_brdf_rough_dielectric_shade;
    bsdf->support_stratified_sampling = false;
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
    bsdf->support_stratified_sampling = false;
}
#endif