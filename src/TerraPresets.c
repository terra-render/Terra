// TerraPresets
#include "TerraPresets.h"

#include <assert.h>

// https://en.wikipedia.org/wiki/Schlick%27s_approximation
static float terra_bsdf_schlick_weight ( float cos_theta ) {
    float m = terra_clamp ( 1.f - cos_theta, 0.f, 1.f );
    float m2 = m * m;
    return m2 * m2 * m;
}

#if 0
TerraFloat3 terra_bsdf_fresnel_schlick ( const TerraFloat3* R0, const TerraFloat3* w, const TerraFloat3* h ) {
    float cos_theta = terra_maxf ( 0.f, terra_dotf3 ( w, h ) );
    TerraFloat3 a = terra_f3_set ( 1 - R0->x, 1 - R0->y, 1 - R0->z );
    a = terra_mulf3 ( &a, powf ( 1 - cos_theta, 5 ) );
    return terra_addf3 ( &a, R0 );
}

TerraFloat3 terra_bsdf_R0 ( float ior, const TerraFloat3* albedo, float metalness ) {
    float f = ( 1.f - ior ) / ( 1.f + ior );
    f *= f;
    TerraFloat3 F0 = terra_f3_set1 ( f );
    F0 = terra_lerpf3 ( &F0, albedo, metalness );
    return F0;
}
#endif

//--------------------------------------------------------------------------------------------------
// Preset: Diffuse [Cosine] weighted sampling (Lambertian)
// http://www.rorydriscoll.com/2009/01/07/better-sampling/
//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_bsdf_diffuse_sample ( const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo ) {
    // cosine weighted hemisphere sampling
    // disk to hemisphere projection
    float r = sqrtf ( e1 );
    float theta = 2 * terra_PI * e2;
    float x = r * cosf ( theta );
    float z = r * sinf ( theta );
    TerraFloat3 wi = terra_f3_set ( x, sqrtf ( terra_maxf ( 0.f, 1 - e1 ) ), z );
    wi = terra_transformf3 ( &surface->transform, &wi );
    wi = terra_normf3 ( &wi );
    return wi;
}

float terra_bsdf_diffuse_pdf ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo ) {
    float NoL = terra_maxf ( 0.f, terra_dotf3 ( &surface->normal, wi ) );
    return NoL / terra_PI;
}

TerraFloat3 terra_bsdf_diffuse_eval ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo ) {
    return terra_mulf3 ( &surface->attributes[TERRA_DIFFUSE_ALBEDO], 1. / terra_PI );
}

void terra_bsdf_diffuse_init ( TerraBSDF* bsdf ) {
    bsdf->sample = terra_bsdf_diffuse_sample;
    bsdf->pdf = terra_bsdf_diffuse_pdf;
    bsdf->eval = terra_bsdf_diffuse_eval;
}

//--------------------------------------------------------------------------------------------------
// Preset: Phong
// http://www.cs.princeton.edu/courses/archive/fall16/cos526/papers/importance.pdf
//--------------------------------------------------------------------------------------------------
void terra_bsdf_phong_calculate_kd_ks ( const TerraShadingSurface* surface, float* kd, float* ks ) {
    float diffuse = terra_maxf ( surface->attributes[TERRA_PHONG_ALBEDO].x +
                                 surface->attributes[TERRA_PHONG_ALBEDO].y +
                                 surface->attributes[TERRA_PHONG_ALBEDO].z,
                                 terra_Epsilon );
    float specular = surface->attributes[TERRA_PHONG_SPECULAR_COLOR].x +
                     surface->attributes[TERRA_PHONG_SPECULAR_COLOR].y +
                     surface->attributes[TERRA_PHONG_SPECULAR_COLOR].z;

    if ( specular > diffuse ) {
        *kd = 0.5f * diffuse / specular;
        *ks = 1.f - *kd;
    } else {
        *ks = 0.5f * specular / diffuse;
        *kd = 1.f - *ks;
    }
}

TerraFloat3 terra_bsdf_phong_sample ( const TerraShadingSurface* surface, float e1, float e2, float e3, const TerraFloat3* wo ) {
    float kd, ks;
    terra_bsdf_phong_calculate_kd_ks ( surface, &kd, &ks );
    TerraFloat3* sample_attrib = &surface->attributes[TERRA_PHONG_SAMPLE_PICK];

    if ( e3 < kd ) {
        // Diffuse hemisphere sample.
        sample_attrib->x = 1.f;
        return terra_bsdf_diffuse_sample ( surface, e1, e2, e3, wo );
    } else {
        // Specular lobe sample.
        sample_attrib->x = -1.f;
        TerraFloat3 wr = terra_mulf3 ( &surface->normal, 2.f * terra_dotf3 ( wo, &surface->normal ) );
        wr = terra_subf3 ( &wr, wo );
        TerraFloat4x4 wr_transform = terra_f4x4_basis ( &wr );
        float phi = 2 * terra_PI * e1;
        float theta = acosf ( powf ( 1.f - e2, 1.f / ( surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x + 1 ) ) );
        float sin_theta = sinf ( theta );
        TerraFloat3 wi = terra_f3_set ( sin_theta * cosf ( phi ), cosf ( theta ), sin_theta * sinf ( phi ) );
        wi = terra_transformf3 ( &wr_transform, &wi );
        return terra_normf3 ( &wi );
    }
}

float terra_bsdf_phong_pdf ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo ) {
    TerraFloat3* sample_attrib = &surface->attributes[TERRA_PHONG_SAMPLE_PICK];
    assert ( sample_attrib->x == 1.f || sample_attrib->x == -1.f );

    if ( sample_attrib->x == 1.f ) {
        return terra_bsdf_diffuse_pdf ( surface, wi, wo );
    } else if ( sample_attrib->x == -1.f ) {
        TerraFloat3 wr = terra_mulf3 ( &surface->normal, 2.f * terra_dotf3 ( wo, &surface->normal ) );
        wr = terra_subf3 ( &wr, wo );
        float cos_alpha = terra_dotf3 ( wi, &wr );
        float n = surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x;
        return ( n + 1 ) / ( 2 * terra_PI ) * powf ( cos_alpha, n );
    } else {
        assert ( false );
    }
}

TerraFloat3 terra_bsdf_phong_eval ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo ) {
    float kd, ks;
    terra_bsdf_phong_calculate_kd_ks ( surface, &kd, &ks );
    float n = surface->attributes[TERRA_PHONG_SPECULAR_INTENSITY].x;
    // Diffuse
    TerraFloat3 diffuse_term = terra_mulf3 ( &surface->attributes[TERRA_PHONG_ALBEDO], kd * 1.f / terra_PI );
    // Specular
    TerraFloat3 reflection_dir = terra_mulf3 ( &surface->normal, 2.f * terra_dotf3 ( wo, &surface->normal ) );
    reflection_dir = terra_subf3 ( &reflection_dir, wo );
    float cos_alpha = terra_dotf3 ( wi, &reflection_dir );
    float cos_n_alpha = powf ( cos_alpha, n );
    TerraFloat3 specular_term = terra_mulf3 ( &surface->attributes[TERRA_PHONG_SPECULAR_COLOR], ks * cos_n_alpha * ( n + 2 ) / ( 2 * terra_PI ) );
    // Final
    TerraFloat3 final_term = terra_addf3 ( &diffuse_term, &specular_term );
    return final_term;
}

void terra_bsdf_phong_init ( TerraBSDF* bsdf ) {
    bsdf->sample = terra_bsdf_phong_sample;
    bsdf->pdf = terra_bsdf_phong_pdf;
    bsdf->eval = terra_bsdf_phong_eval;
}

//--------------------------------------------------------------------------------------------------
// Preset: Disney
//--------------------------------------------------------------------------------------------------
// https://github.com/wdas/brdf/blob/master/src/brdfs/disney.brdf
// https://schuttejoe.github.io/post/disneybsdf/

TerraFloat3 terra_bsdf_disney_tint ( TerraFloat3 base_color ) {
    TerraFloat3 luminance_coeffs = terra_f3_set ( 0.3, 0.6, 1.0 );
    float luminance = terra_dotf3 ( &luminance_coeffs, &base_color );

    if ( luminance == 0 ) {
        return terra_f3_one;
    }

    return terra_mulf3 ( &base_color, 1.f / luminance );
}

TerraFloat3 terra_bsdf_disney_sheen ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo, const TerraFloat3* h ) {
    float sheen = surface->attributes[TERRA_DISNEY_SHEEN].x;
    float sheen_tint = surface->attributes[TERRA_DISNEY_SHEEN].y;
    TerraFloat3 base_color = surface->attributes[TERRA_DISNEY_BASE_COLOR];

    if ( sheen <= 0.f ) {
        return terra_f3_zero;
    }

    float HdotL = terra_dotf3 ( h, wi );
    TerraFloat3 tint = terra_bsdf_disney_tint ( base_color );
    TerraFloat3 onef3 = terra_f3_one;
    TerraFloat3 sheenf3 = terra_lerpf3 ( &onef3, &tint, sheen_tint );
    sheenf3 = terra_mulf3 ( &sheenf3, sheen * terra_bsdf_schlick_weight ( HdotL ) );
    return sheenf3;
}

float terra_bsdf_disney_GTR2_aniso ( float NdotH, float HdotX, float HdotY, float ax, float ay ) {
    float x = HdotX / ax;
    float y = HdotY / ay;
    float s = x * x + y * y + NdotH * NdotH;
    return 1.f / ( terra_PI * ax * ay * s * s );
}

float terra_bsdf_disney_smithG_GGX_aniso ( float NdotV, float VdotX, float VdotY, float ax, float ay ) {
    float x = VdotX * ax;
    float y = VdotY * ay;
    return 1.f / ( NdotV + sqrtf ( x * x + y * y + NdotV * NdotV ) );
}

float terra_bsdf_disney_smithG_GGX ( float NdotV, float alphaG ) {
    float a = alphaG * alphaG;
    float b = NdotV * NdotV;
    return 1.f / ( NdotV + sqrtf ( a + b - a * b ) );
}

float terra_bsdf_disney_GTR1 ( float NdotH, float a ) {
    if ( a >= 1 ) {
        return 1 / terra_PI;
    }

    float a2 = a * a;
    float t = 1 + ( a2 - 1 ) * NdotH * NdotH;
    return ( a2 - 1 ) / ( terra_PI * logf ( a2 ) * t );
}

float terra_bsdf_disney_GTR2 ( float NdotH, float a ) {
    float a2 = a * a;
    float t = 1 + ( a2 - 1 ) * NdotH * NdotH;
    return ( a2 - 1 ) / ( terra_PI * t * t );
}

/*
    [ base_color.rgb ]
    [ specular | specular_tint | _ ]
    [ sheen | sheen_tint | _ ]
    [ clearcoat | clearcoat_gloss | _ ]
    [ metalness | roughness | _ ]
    [ anisotropic | subsurface  | _ ]
*/

TerraFloat3 terra_bsdf_disney_eval ( const TerraShadingSurface* surface, const TerraFloat3* wi, const TerraFloat3* wo ) {
    float NdotL = terra_dotf3 ( &surface->normal, wi );
    float NdotV = terra_dotf3 ( &surface->normal, wo );

    if ( NdotL < 0 || NdotV < 0 ) {
        return terra_f3_zero;
    }

    // Half vector
    TerraFloat3 H = terra_addf3 ( wi, wo );
    H = terra_normf3 ( &H );
    float NdotH = terra_dotf3 ( &surface->normal, &H );
    float LdotH = terra_dotf3 ( wi, &H );
    // Base Color Luminance
    TerraFloat3 base_color = surface->attributes[TERRA_DISNEY_BASE_COLOR];
    TerraFloat3 lum_weights = terra_f3_set ( 0.3, 0.6, 1 );
    float base_color_lum = terra_dotf3 ( &base_color, &lum_weights );
    // Tint
    TerraFloat3 tint = base_color_lum > 0 ? terra_mulf3 ( &base_color, 1.f / base_color_lum ) : terra_f3_one;
    // Spec0
    float specular = 0; // =
    float specular_tint = 0; // =
    float metalness = 0; // =
    TerraFloat3 spec0 = terra_lerpf3 ( &terra_f3_one, &tint, specular_tint );
    spec0 = terra_mulf3 ( &spec0, specular * 0.8f );
    spec0 = terra_lerpf3 ( &spec0, &base_color, metalness );
    // Sheen
    float sheen_tint = 0; // =
    TerraFloat3 sheen_c = terra_lerpf3 ( &terra_f3_one, &tint, sheen_tint );
    // Diffuse Fresnel
    float roughness = 0; // =
    float FL = terra_bsdf_schlick_weight ( NdotL );
    float FV = terra_bsdf_schlick_weight ( NdotV );
    float Fd90 = 0.5f + 2 * LdotH * LdotH * roughness;
    float Fd = terra_lerp ( 1.f, Fd90, FL ) * terra_lerp ( 1.f, Fd90, FV );
    // Subsurface Scattering
    float Fss90 = LdotH * LdotH * roughness;
    float Fss = terra_lerp ( 1.f, Fss90, FL ) * terra_lerp ( 1.f, Fss90, FV );
    float ss = 1.25f * ( Fss * ( 1.f / ( NdotL * NdotV ) - 0.5f ) + 0.5f );
    // Specular
    float anisotropic = 0; // =
    float aspect = sqrtf ( 1.f - anisotropic * 0.9f );
    float ax = terra_maxf ( 0.001f, roughness * roughness / aspect );
    float ay = terra_maxf ( 0.001f, roughness * roughness * aspect );
    TerraFloat3 X = terra_f4x4_get_tangent ( &surface->transform );
    TerraFloat3 Y = terra_f4x4_get_bitangent ( &surface->transform );
    float Ds = terra_bsdf_disney_GTR2_aniso ( NdotH, terra_dotf3 ( &H, &X ), terra_dotf3 ( &H, &Y ), ax, ay );
    float FH = terra_bsdf_schlick_weight ( LdotH );
    TerraFloat3 Fs = terra_lerpf3 ( &spec0, &terra_f3_one, FH );
    float Gs = terra_bsdf_disney_smithG_GGX_aniso ( NdotL, terra_dotf3 ( wi, &X ), terra_dotf3 ( wi, &Y ), ax, ay );
    Gs *= terra_bsdf_disney_smithG_GGX_aniso ( NdotV, terra_dotf3 ( wo, &X ), terra_dotf3 ( wo, &Y ), ax, ay );
    // Sheen
    float sheen_p = 0; // =
    TerraFloat3 sheen = terra_mulf3 ( &sheen_c, FH * sheen_p );
    // Clearcoat
    float clearcoat_gloss = 0; // =
    float Dr = terra_bsdf_disney_GTR1 ( NdotH, terra_lerp ( 0.1f, 0.001f, clearcoat_gloss ) );
    float Fr = terra_lerp ( 0.04f, 1.f, FH );
    float Gr = terra_bsdf_disney_smithG_GGX ( NdotL, 0.25f ) * terra_bsdf_disney_smithG_GGX ( NdotV, 0.25f );
    // Result
    float subsurface = 0; // =
    float clearcoat = 0; // =
    TerraFloat3 result_a = terra_mulf3 ( &base_color, 1.f / terra_PI * terra_lerp ( Fd, ss, subsurface ) );
    result_a = terra_addf3 ( &result_a, &sheen );
    result_a = terra_mulf3 ( &result_a, 1 - metalness );
    TerraFloat3 result_b = terra_mulf3 ( &Fs, Gs * Ds );
    TerraFloat3 result_c = terra_f3_set1 ( 0.25f * clearcoat * Gr * Fr * Dr );
    TerraFloat3 result = terra_addf3 ( &result_b, &result_c );
    result = terra_addf3 ( &result, &result_a );
    return result;
}

#if 0
//--------------------------------------------------------------------------------------------------
// Preset: Rough-dielectric = Diffuse + Microfacet GGX specular
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
//--------------------------------------------------------------------------------------------------
float terra_brdf_ctggx_chi ( float val ) {
    return val <= 0.f ? 0.f : 1.f;
}

float terra_brdf_ctggx_G1 ( const TerraFloat3* v, const TerraFloat3* n, const TerraFloat3* h, float alpha2 ) {
    float VoH = terra_dotf3 ( v, h );
    float VoN = terra_dotf3 ( v, n );
    float chi = terra_brdf_ctggx_chi ( VoH / VoN );
    float VoH2 = VoH * VoH;
    float tan2 = ( 1.f - VoH2 ) / VoH2;
    return ( chi * 2 ) / ( sqrtf ( 1 + alpha2 * tan2 ) + 1 );
}

float terra_brdf_ctggx_D ( float NoH, float alpha2 ) {
    float NoH2 = NoH * NoH;
    float den = NoH2 * alpha2 + ( 1 - NoH2 );
    return ( terra_brdf_ctggx_chi ( NoH ) * alpha2 ) / ( terra_PI * den * den );
}

TerraFloat3 terra_bsdf_rough_dielectric_sample ( const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3 ) {
    state->roughness = terra_eval_attribute ( &material->roughness, &ctx->texcoord ).x;
    state->metalness = terra_eval_attribute ( &material->metalness, &ctx->texcoord ).x;
    const float pd = 1.f - state->metalness;

    if ( e3 <= pd ) {
        TerraFloat3 light = terra_bsdf_diffuse_sample ( material, NULL, ctx, e1, e2, 0 );
        state->half_vector = terra_addf3 ( &light, &ctx->view );
        state->half_vector = terra_normf3 ( &state->half_vector );
        return light;
    } else {
        float alpha = state->roughness;
        // TODO: Can be optimized
        float theta = atanf ( ( alpha * sqrtf ( e1 ) ) / sqrtf ( 1 - e1 ) );
        float phi = 2 * terra_PI * e2;
        float sin_theta = sinf ( theta );
        state->half_vector = terra_f3_set ( sin_theta * cosf ( phi ), cosf ( theta ), sin_theta * sinf ( phi ) );
        state->half_vector = terra_transformf3 ( &ctx->rot, &state->half_vector );
        state->half_vector = terra_normf3 ( &state->half_vector );
        float HoV = terra_maxf ( 0.f, terra_dotf3 ( &state->half_vector, &ctx->view ) );
        TerraFloat3 r = terra_mulf3 ( &state->half_vector, 2 * HoV );
        return terra_subf3 ( &r, &ctx->view );
    }
}

float terra_bsdf_rough_dielectric_weight ( const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx ) {
    float alpha = state->roughness;
    float alpha2 = alpha * alpha;
    float NoH = terra_dotf3 ( &ctx->normal, &state->half_vector );
    float weight_specular = terra_brdf_ctggx_D ( NoH, alpha2 ) * NoH;
    float weight_diffuse = terra_bsdf_diffuse_weight ( material, NULL, light, ctx );
    const float pd = 1.f - state->metalness;
    const float ps = 1.f - pd;
    return weight_diffuse * pd + weight_specular * ps;
}

TerraFloat3 terra_bsdf_rough_dielectric_shade ( const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx ) {
    TerraFloat3 albedo = terra_eval_attribute ( &material->albedo, &ctx->texcoord );
    TerraFloat3 F_0 = terra_F_0 ( material->ior, &albedo, state->metalness );
    TerraFloat3 ks = terra_fresnel ( &F_0, &ctx->view, &state->half_vector );
    // Specular
    float NoL = terra_maxf ( terra_dotf3 ( &ctx->normal, light ), 0.f );
    float NoV = terra_maxf ( terra_dotf3 ( &ctx->normal, &ctx->view ), 0.f );
    float NoH = terra_maxf ( terra_dotf3 ( &ctx->normal, &state->half_vector ), 0.f );
    float alpha = state->roughness;
    float alpha2 = alpha * alpha;
    // Fresnel (Schlick approx)
    // float metalness = terra_eval_attribute(&material->metalness, &ctx->texcoord).x;
    // D
    float D = terra_brdf_ctggx_D ( NoH, alpha2 );
    // G
    float G = terra_brdf_ctggx_G1 ( &ctx->view, &ctx->normal, &state->half_vector, alpha2 ) *
              terra_brdf_ctggx_G1 ( light, &ctx->normal, &state->half_vector, alpha2 );
    float den_CT = terra_minf ( ( 4 * NoL * NoV ) + 0.05f, 1.f );
    TerraFloat3 specular_term = terra_mulf3 ( &ks, G * D / den_CT );
    // Diffuse
    TerraFloat3 diffuse_term = terra_bsdf_diffuse_shade ( material, NULL, light, ctx );
    // Scaling contributes
    const float pd = 1.f - state->metalness;
    const float ps = 1.f - pd;
    TerraFloat3 diffuse_factor = terra_f3_set ( 1.f - ks.x, 1.f - ks.y, 1.f - ks.z );
    diffuse_factor = terra_mulf3 ( &diffuse_factor, ( 1.f - state->metalness ) * pd );
    diffuse_term = terra_pointf3 ( &diffuse_term, &diffuse_factor );
    specular_term = terra_mulf3 ( &specular_term, ps );
    TerraFloat3 reflectance = terra_addf3 ( &diffuse_term, &specular_term );
    return terra_mulf3 ( &reflectance, NoL );
}

void terra_bsdf_init_rough_dielectric ( TerraBSDF* bsdf ) {
    bsdf->sample = terra_bsdf_rough_dielectric_sample;
    bsdf->weight = terra_bsdf_rough_dielectric_weight;
    bsdf->shade = terra_bsdf_rough_dielectric_shade;
}

//--------------------------------------------------------------------------------------------------
// Preset: Perfect glass
//--------------------------------------------------------------------------------------------------
TerraFloat3 terra_bsdf_glass_sample ( const TerraMaterial* material, TerraShadingState* state, const TerraShadingContext* ctx, float e1, float e2, float e3 ) {
    TerraFloat3 normal = ctx->normal;
    TerraFloat3 incident = terra_negf3 ( &ctx->view );
    float n1, n2;
    float cos_i = terra_dotf3 ( &normal, &incident );

    // n1 to n2. Flipping in case we are inside a material
    if ( cos_i > 0.f ) {
        n1 = material->ior;
        n2 = terra_ior_air;
        normal = terra_negf3 ( &normal );
    } else {
        n1 = terra_ior_air;
        n2 = material->ior;
        cos_i = -cos_i;
    }

    // Reflection
    TerraFloat3 refl = terra_mulf3 ( &normal, 2 * terra_dotf3 ( &normal, &incident ) );
    refl = terra_subf3 ( &incident, &refl );
    // TIR ~ Faster version than asin(n2 / n1)
    float nni = ( n1 / n2 );
    float cos_t = 1.f - nni * nni * ( 1.f - cos_i * cos_i );

    if ( cos_t < 0.f ) {
        state->fresnel = 1.f;
        return refl;
    }

    cos_t = sqrtf ( cos_t );
    // Unpolarized schlick fresnel
    float t = 1.f - ( n1 <= n2 ? cos_i : cos_t );
    float R0 = ( n1 - n2 ) / ( n1 + n2 );
    R0 *= R0;
    float R = R0 + ( 1 - R0 ) * ( t * t * t * t * t );

    // Russian roulette reflection/transmission
    if ( e3 < R ) {
        state->fresnel = R;
        return refl;
    }

    // Transmitted direction
    // the '-' is because our view is the opposite of the incident direction
    TerraFloat3 trans_v = terra_mulf3 ( &normal, ( n1 / n2 ) * cos_i - cos_t );
    TerraFloat3 trans_n = terra_mulf3 ( &incident, n1 / n2 );
    TerraFloat3 trans = terra_addf3 ( &trans_v, &trans_n );
    trans = terra_normf3 ( &trans );
    state->fresnel = 1 - R;
    return trans;
}

float terra_bsdf_glass_weight ( const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx ) {
    return state->fresnel;
}

TerraFloat3 terra_bsdf_glass_shade ( const TerraMaterial* material, TerraShadingState* state, const TerraFloat3* light, const TerraShadingContext* ctx ) {
    TerraFloat3 albedo = terra_eval_attribute ( &material->albedo, &ctx->texcoord );
    return terra_mulf3 ( &albedo, state->fresnel );
}

void terra_bsdf_init_glass ( TerraBSDF* bsdf ) {
    bsdf->sample = terra_bsdf_glass_sample;
    bsdf->weight = terra_bsdf_glass_weight;
    bsdf->shade = terra_bsdf_glass_shade;
}
#endif