// Header
#include <Terra.h>

// Terra private API
#include "TerraPrivate.h"

// libc
#include <string.h> // memset

size_t terra_linear_index ( uint16_t x, uint16_t y, uint16_t w ) {
    return ( size_t ) ( y ) * w + x;
}

/*
    Returns the number of mipmap levels for a wxh texture.
*/
size_t terra_mips_count ( uint16_t w, uint16_t h ) {
    return 1 + floor ( log2 ( TERRA_MAX ( ( size_t ) w, ( size_t ) h ) ) );
}

/*
    Returns the number of anisotropically scaled mipmaps for a
    wxh texture. Does not include isotropic scaled maps.
*/
size_t terra_rips_count ( uint16_t w, uint16_t h ) {
    size_t n_mips = terra_mips_count ( w, h ) - 1;
    return n_mips * n_mips;
}


/*
    Actual texture creation routine. The public API should be used to generate
    textures.
    <depth> should either be 1 for RGBA8_UNORM or 4 for RGBA32_FLOAT formats.
*/
bool terra_texture_create_any ( TerraTexture* texture, const void* data, size_t width, size_t height, size_t depth, size_t components, size_t sampler, TerraTextureAddress address ) {
    TERRA_ASSERT ( texture && data );
    TERRA_ASSERT ( width > 0 && height > 0 && components > 0 );
    TERRA_ASSERT ( width < ( uint16_t ) - 1 && height < ( uint16_t ) - 1 );

    // Initializing
    if ( sampler & kTerraSamplerAnisotropic ) {
        sampler |= kTerraSamplerTrilinear;
    }

    texture->width = ( uint16_t ) width;
    texture->height = ( uint16_t ) height;
    texture->sampler = ( uint8_t ) sampler;
    texture->address = address;
    texture->mipmaps = nullptr;
    texture->ripmaps = nullptr;

    // Calculating mip levels
    size_t n_mipmaps = terra_texture_mips_count ( texture );
    size_t n_ripmaps = terra_texture_rips_count ( texture );

    texture->mipmaps = ( TerraMap* ) terra_malloc ( sizeof ( TerraMap ) * n_mipmaps );
    TERRA_ZEROMEM_ARRAY ( texture->mipmaps, n_mipmaps );

    texture->mipmaps[0] = terra_map_create_mip0 ( width, height, components, depth, data );
    TERRA_ASSERT ( texture->mipmaps[0].data );

    // Converting to srgb
    if ( texture->sampler & kTerraSamplerSRGB ) {
        terra_map_linearize_srgb ( texture->mipmaps );
    }

    // Isotropic downscaling
    for ( size_t i = 1; i < n_mipmaps; ++i ) {
        terra_map_init ( texture->mipmaps + i, texture->width >> i, texture->height >> i );
        terra_map_dpid_downscale ( texture->mipmaps + i, texture->mipmaps );
    }

    // Anisotropic downscaling
    if ( n_ripmaps > 0 ) {
        size_t downscaled_mip_lvls = n_mipmaps - 1;
        texture->ripmaps = ( TerraMap* ) terra_malloc ( sizeof ( TerraMap ) * n_ripmaps );

        for ( size_t i = 0; i < n_ripmaps; ++i ) {
            size_t mip_lvl_w = i % downscaled_mip_lvls;
            size_t mip_lvl_h = i / downscaled_mip_lvls;

            terra_map_init ( texture->ripmaps, texture->width >> mip_lvl_w, texture->height >> mip_lvl_h );
            terra_map_dpid_downscale ( texture->ripmaps + i, texture->mipmaps );
        }
    }

    return true;
}

/*
    Returns the attribute evaluation routine associated with the sampler. This avoids some
    of the branching in the textures code, but some dynamic dispatches are still performed for
    other attributes.
*/
TerraAttributeEval terra_texture_sampler ( const TerraTexture* texture ) {
    if ( texture->sampler & kTerraSamplerSpherical ) {
        return terra_texture_sampler_spherical;
    }

    if ( texture->sampler & kTerraSamplerAnisotropic ) {
        return terra_texture_sampler_anisotropic;
    }

    if ( texture->sampler & kTerraSamplerTrilinear ) {
        return terra_texture_sampler_mipmaps;
    }

    return terra_texture_sampler_mip0;
}

size_t terra_texture_mips_count ( const TerraTexture* texture ) {
    if ( texture->sampler & kTerraSamplerTrilinear ) {
        return terra_mips_count ( texture->width, texture->height );
    }

    return 1;
}

size_t terra_texture_rips_count ( const TerraTexture* texture ) {
    if ( texture->sampler & kTerraSamplerAnisotropic ) {
        return terra_rips_count ( texture->width, texture->height );
    }

    return 0;
}

/*
    Allocates enough memory for the specified dimensions.
*/
void terra_map_init ( TerraMap* map, uint16_t w, uint16_t h ) {
    map->data = ( float* ) terra_malloc ( sizeof ( float ) * w * h * 4 );
    map->width = w;
    map->height = h;
    TERRA_ASSERT ( map->data );

    // Terra itself doesn't use alpha channels, but this is useful for
    // visualization and exporting. Might consider wrapping in #ifdef
    // (No point in setting the whole image).
    for ( size_t i = 0; i < ( size_t ) map->width * map->height; ++i ) {
        map->data[i * 4 + 3] = 1.f;
    }
}

/*
    Releases memory.
*/
void terra_map_destroy ( TerraMap* map ) {
    if ( map->data != nullptr ) {
        terra_free ( map->data );
    }

    TERRA_ZEROMEM ( map );
}

size_t terra_map_stride ( const TerraMap* map ) {
    return sizeof ( float ) * map->width * 4;
}

/*
    Performs a discrete (window size) convolution between <src> and <kernel> and
    writes the result in <dst>. It is performed only on the first 3 channels.
    <src> and <dst> are required to have the same size, but cannot point
    to the same object.

    Currently a maximum kernel size of 8(x8) is supported. Lifting this is
    pretty straightforward as it's caused by the mask used to check the
    kernel boundaries on the source signal.
    The mask can be replaced with a counter or similar, but I like this
    implementation more and profiling is required (TODO) before changing approach.

    Misc improvements ideas:
        - SIMD
        - Swap mask with counter
        - Rethink window approach/branching
        - Rewrite without `int => uint16_t`
        - Exploit kernel symmetry to cache values (or wherever imagination goes)
        - Integrate with sampling (lift src.dimensions == dst.dimensions)

    but really.. profile this first.
*/
void terra_map_convolution ( TerraMap* TERRA_RESTRICT dst, const TerraMap* TERRA_RESTRICT src, const float* kernel, size_t kernel_size ) {
    TERRA_ASSERT ( dst != src );
    TERRA_ASSERT ( dst->data != src->data ); // This is an encapsulation assumption on TerraMap

    size_t n_vals = kernel_size * kernel_size;
    float scale = 0.f;

    for ( size_t i = 0; i < n_vals; ++i ) {
        scale += kernel[i];
    }

    scale = 1.f / scale;

    int dk = ( int ) kernel_size / 2;
    int iw = ( int ) dst->width;
    int ih = ( int ) dst->height;

    for ( int y = 0; y < ih; ++y ) {
        for ( int x = 0; x < iw; ++x ) {
            uint64_t mask    = ( uint64_t ) - 1 ; // Keeping track how many window values are in range
            int ik           = 0;                 // kernel linear index
            float norm_scale = 0.f;               // adjusted norm if the window is out of range
            TerraFloat3 sum  = TERRA_F3_ZERO;     // accumulator

            for ( int ky = y - dk; ky <= y + dk; ++ky ) {
                for ( int kx = x - dk; kx <= x + dk; ++kx ) {
                    int off = ( int ) ( kx < 0 || ky < 0 || kx >= iw || ky >= ih );
                    mask &= ~ ( off << ik );

                    if ( off ) { // I think nicer than multiplying by (1 - off)
                        continue;
                    }

                    TerraFloat3 val = terra_map_read_texel ( src, ( uint16_t ) kx, ( uint16_t ) y );
                    val = terra_mulf3 ( &val, kernel[ik] );
                    sum = terra_addf3 ( &sum, &val );
                    norm_scale += kernel[ik];
                }
            }

            size_t idx = terra_linear_index ( x, y, iw );
            sum = terra_mulf3 ( &sum, ( mask == 0xffffffff ) ? scale : 1. / norm_scale );
            terra_map_write_texel ( dst, x, y, &sum );
        }
    }
}

/*
    Converts from SRGB to linear SRGB.
    sRGB ICC Profile http://color.org/chardata/rgb/sRGB.pdf
*/
void terra_map_linearize_srgb ( TerraMap* map ) {
    TERRA_ASSERT ( map && map->data );

    const float line_scale = 1.f / 12.92f;
    const float exp_scale = 1.f / 1.055f;
    const float thr = 0.04045f;

    const size_t n_pixels = map->width * map->height * 4;
    float* rw = map->data;

    for ( size_t i = 0; i < n_pixels; ++i ) {
        if ( i & 3 == 0 ) {
            continue;
        }

        rw[i] = rw[i] <= thr ? rw[i] * line_scale : powf ( rw[i] + 0.055f, 2.4f );
    }
}

/*
    Creates the first miplevel from the input texture. It linearizes the pixel
    values and pads to 4 components, regardless of input format.
*/
TerraMap terra_map_create_mip0 ( size_t width, size_t height, size_t components, size_t depth, const void* data ) {
    TERRA_ASSERT ( depth == 1 || depth == 4 );

    size_t size = 4 * width * height;

    TerraMap map;
    map.width  = ( uint16_t ) width;
    map.height = ( uint16_t ) height;
    map.data   = ( float* ) terra_malloc ( sizeof ( float ) * size );

    if ( depth == 1 ) {
        uint8_t*       w = ( uint8_t* ) map.data;
        const uint8_t* r = ( const uint8_t* ) data;

        for ( size_t i = 0; i < width * height; ++i ) {
            int c = 0;

            // Copying
            while ( c++ < ( int ) components ) {
                * ( w++ ) = * ( r++ );
            }

            // Padding
            c = 5 - c;

            while ( c-- > 0 ) {
                * ( w++ ) = * ( r - components );
            }
        }
    }

    if ( depth == 4 ) {
        const float* r = ( float* ) data;

        for ( size_t i = 0; i < width * height * components; i += components ) {
            if ( components >= 2 ) {
                memcpy ( map.data + i, r + i, sizeof ( float ) * components );
            } else {
                map.data[i + 0] = r[i];
                map.data[i + 1] = r[i];
                map.data[i + 2] = r[i];
                map.data[i + 3] = r[i];
            }
        }
    }

    return map;
}

/*
    Pixel coordinates to pixel value.
*/
TerraFloat3 terra_map_read_texel ( const TerraMap* map, uint16_t x, uint16_t y ) {
    x = TERRA_MIN ( x, map->width - 1 );
    y = TERRA_MIN ( y, map->height - 1 );
    size_t lin_idx =  terra_linear_index ( x, y, map->width );
    return terra_f3_ptr ( map->data + lin_idx * 4 );
}

void terra_map_write_texel ( TerraMap* map, uint16_t x, uint16_t y, const TerraFloat3* rgb ) {
    x = TERRA_MIN ( x, map->width - 1 );
    y = TERRA_MIN ( y, map->height - 1 );
    size_t lin_idx = terra_linear_index ( x, y, map->width );
    memcpy ( map->data + lin_idx * 4, rgb, sizeof ( TerraFloat3 ) );
}

float terra_texture_address_wrap ( float v ) {
    return v < 0.f ? fmodf ( -v, 1.f ) : ( v > 1.f ? fmodf ( v, 1.f ) : v );
}

float terra_texture_address_mirror ( float v ) {
    return v < 0.f ? 1.f - fmodf ( -v, 1.f ) : ( v > 1.f ? 1.f - fmodf ( v, 1.f ) : v );
}

float terra_texture_address_clamp ( float v ) {
    return v < 0.f ? 0.f : ( v > 1.f ? 1.f : v );
}

typedef float ( *terra_texture_address_fn ) ( float );
static terra_texture_address_fn address_fns[] = {
    terra_texture_address_wrap,
    terra_texture_address_mirror,
    terra_texture_address_clamp,
};

/*
    Converts the real \in [0 1] uv coordinate to the pixel equivalent.
    It centers the pixel and processes out of range addresses.
*/
void terra_map_texel_int ( const TerraMap* map, TerraTextureAddress address, const TerraFloat2* uv, uint16_t* x, uint16_t* y ) {
    const float w = ( float ) map->width;
    const float h = ( float ) map->height;
    const float iw = 1.f / w;
    const float ih = 1.f / h;

    const float rx = uv->x - .5f * iw;
    const float ry = uv->y - .5f * ih;

    *x = ( uint16_t ) ( address_fns[address] ( rx ) * w );
    *y = ( uint16_t ) ( address_fns[address] ( ry ) * h );
}

/*
    Nearest filter
*/
TerraFloat3 terra_map_sample_nearest ( const TerraMap* map, const TerraFloat2* uv, TerraTextureAddress address ) {
    const float iw = 1.f / map->width;
    const float ih = 1.f / map->height; // Maybe cache ?
    uint16_t x, y;
    terra_map_texel_int ( map, address, uv, &x, &y );
    return terra_map_read_texel ( map, x, y );
}

/*
    Bilinear filter
*/
TerraFloat3 terra_map_sample_bilinear ( const TerraMap* map, const TerraFloat2* _uv, TerraTextureAddress address ) {
    TerraFloat2 uv = terra_f2_set ( _uv->x * map->width, _uv->y * map->height );

    uint16_t x1, y1;
    terra_map_texel_int ( map, address,  _uv, &x1, &y1 );

    uint16_t x2 = x1 + 1;
    uint16_t y2 = y1;

    uint16_t x3 = x1;
    uint16_t y3 = y1 + 1;

    uint16_t x4 = x1 + 1;
    uint16_t y4 = y1 + 1;

    TerraFloat3 t1 = terra_map_read_texel ( map, x1, y1 );
    TerraFloat3 t2 = terra_map_read_texel ( map, x2, y2 );
    TerraFloat3 t3 = terra_map_read_texel ( map, x3, y3 );
    TerraFloat3 t4 = terra_map_read_texel ( map, x4, y4 );

    float ix = floorf ( uv.x );
    float iy = floorf ( uv.y );
    float w_u = uv.x - ix;
    float w_v = uv.y - iy;
    float w_ou = 1.f - w_u;
    float w_ov = 1.f - w_v;

    TerraFloat3 sample;
    sample.x = ( t1.x * w_ou + t2.x * w_u ) * w_ov + ( t3.x * w_ou + t4.x * w_u ) * w_v;
    sample.y = ( t1.y * w_ou + t2.y * w_u ) * w_ov + ( t3.y * w_ou + t4.y * w_u ) * w_v;
    sample.z = ( t1.z * w_ou + t2.z * w_u ) * w_ov + ( t3.z * w_ou + t4.z * w_u ) * w_v;
    return sample;
}

// Dispatch
typedef TerraFloat3 ( *map_sample_fn ) ( const TerraMap* map, const TerraFloat2* uv, TerraTextureAddress address );
static map_sample_fn sample_fns[] = {
    terra_map_sample_nearest,
    terra_map_sample_bilinear
};

const TerraMap* terra_texture_mip ( const TerraTexture* texture, int mip ) {
    return texture->mipmaps + mip;
}

/*
    Samples a specific miplevel at <u v>
*/
TerraFloat3 terra_texture_sample_mip ( const TerraTexture* texture, int mip, const TerraFloat2* uv ) {
    return sample_fns[texture->sampler & 0x1] ( terra_texture_mip ( texture, mip ), uv, ( TerraTextureAddress ) texture->address );
}

/*

*/
TERRA_TEXTURE_SAMPLER ( mip0 ) {
    return terra_texture_sample_mip ( ( TerraTexture* ) state, 0, ( const TerraFloat2* ) addr );
}

/*

*/
TERRA_TEXTURE_SAMPLER ( mipmaps ) {
    return terra_f3_set ( 1.f, 1.f, 1.f );
}

/*

*/
TERRA_TEXTURE_SAMPLER ( anisotropic ) {
    return terra_f3_set ( 1.f, 1.f, 1.f );
}

/*

*/
TERRA_TEXTURE_SAMPLER ( spherical ) {
    return terra_f3_set ( 1.f, 1.f, 1.f );
}


void terra_dpid_compute_guidance ( TerraMap* I_g, const TerraMap* I ) {
    const float step_u = 1.f / I_g->width;
    const float step_v = 1.f / I_g->height;

    // Downscaling
    for ( uint16_t y = 0; y < I_g->width; ++y ) {
        for ( uint16_t x = 0; x < I_g->height ; ++x ) {
            TerraFloat2 uv = terra_f2_set ( step_u * x, step_v * y );
            TerraFloat3 rgb = terra_map_sample_bilinear ( I, &uv, kTerraTextureAddressWrap );
            terra_map_write_texel ( I_g, x, y, &rgb );
        }
    }

    TerraMap tmp;
    terra_map_init ( &tmp, I_g->width, I_g->height );

    // Box filter
    const float box_kernel[] = {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1
    };
    terra_map_convolution ( &tmp, I_g, box_kernel, 3 );

    // Guidance convolution
    const float guidance_kernel[] = {
        1, 2, 1,
        2, 4, 2,
        1, 2, 1
    };
    terra_map_convolution ( I_g, &tmp, guidance_kernel, 3 );

    terra_map_destroy ( &tmp );
}

/*
    Bilateral filter. It's fundamentally a convolution which accounts for the
    variation of photometric intensity between the pixels in the window. It is joint
    as rather than comparing against the value at the center of the window, it uses
    the previously computed guidance map as value to diff with.

    Some very extensive notes can be found here: https://people.csail.mit.edu/sparis/bf_course/course_notes.pdf

    The only difference is that since we are downscaling an 2D image, the window size is
    not a parameter, but is given by the region of O which gets mapped into I. Most of the code
    in the inner loop is to make sure that the pixels in the window are weighted correctly if the
    scaling factor is a real number.
*/
void terra_dpid_joint_bilateral_filter ( TerraMap* O, const TerraMap* I_g, const TerraMap* I ) {
    const float lambda = 1.f;
    const float norm_factor = 1.f / sqrtf ( 3.f ); // RGB_FLOAT32 color space normalization factor

    const uint32_t iw = ( uint32_t ) I->width;
    const uint32_t ih = ( uint32_t ) I->height;

    // Maps [0 1] uv coordinates from the output image to the input image
    // simplified from (dim(I) / dim(O)) / dim(I)
    const float i_stepu = 1.f / O->width;
    const float i_stepv = 1.f / O->height;

    // Weight correction for window border, see weightsx, weightsy
    const float sx = ( float ) iw / O->width;
    const float sy = ( float ) ih / O->height;
    const float weight_left_adj = terra_is_integer ( sx ) ? 1.f : 0.f;
    const float weight_top_adj = terra_is_integer ( sy ) ? 1.f : 0.;

    for ( size_t y = 0; y < O->height; ++y ) {
        for ( size_t x = 0; x < O->width; ++x ) {
            // Pixel source window for O(x, y)
            const TerraFloat2 uv_tl = terra_f2_set ( i_stepu * x, i_stepv * y );
            const TerraFloat2 uv_br = terra_f2_set ( i_stepu * ( x + 1 ), i_stepv * ( y + 1 ) );

            const TerraFloat2 bwr = terra_f2_set ( uv_tl.x * iw, uv_br.x * iw );
            const TerraFloat2 bhr = terra_f2_set ( uv_tl.y * ih, uv_br.y * ih );

            // Fraction of the source pixel for the window boundaries
            TerraFloat2 weightsx = terra_f2_set ( bwr.x - ceilf ( bwr.x ), bwr.y - floorf ( bwr.y ) );
            TerraFloat2 weightsy = terra_f2_set ( bhr.x - ceilf ( bhr.x ), bhr.y - floorf ( bhr.y ) );

            // If we have calculated the window correctly enough the last indices would be out of range
            // (width, height) and would have weight 0 anyway
            TERRA_ASSERT ( x != O->width - 1 || weightsx.y < TERRA_EPS );
            TERRA_ASSERT ( y != O->height - 1 || weightsy.y < TERRA_EPS );

            // With the calculations above if the scaling factor is integer, top and left would have weight 0.
            // Correcting for this. This is more of an indexing (wx, wy) problem, reformulating with signed values
            // could make this step unnecessary (TODO?)
            weightsx.x += weight_left_adj;
            weightsy.x += weight_top_adj;

            const TerraUInt2 wnd_rangex = terra_ui2_set ( ( uint32_t ) ( uv_tl.x * iw ), ( uint32_t ) ( uv_br.x * iw ) );
            const TerraUInt2 wnd_rangey = terra_ui2_set ( ( uint32_t ) ( uv_tl.y * ih ), ( uint32_t ) ( uv_br.y * ih ) );

            TerraFloat3 acc = TERRA_F3_ZERO;
            float acc_norm = 0.f;
            const TerraFloat3 Ip_g = terra_map_read_texel ( I_g, x, y );

            for ( uint32_t wy = wnd_rangey.x; wy <= wnd_rangey.y; ++wy ) {
                for ( uint32_t wx = wnd_rangex.x; wx <= wnd_rangex.y; ++wx ) {
                    // (TODO) This is ridicolous
                    const bool is_left  = wx == wnd_rangex.x;
                    const bool is_right = wx == wnd_rangex.y;
                    const bool is_top   = wy == wnd_rangey.x;
                    const bool is_down  = wy == wnd_rangey.x;

                    const float weightx = is_left ? weightsx.x : ( is_right ? weightsx.y : 1.f );
                    const float weighty = is_top ? weightsy.x : ( is_down ? weightsy.y : 1.f );
                    const float weight = weightx * weighty;

                    if ( weight < TERRA_EPS ) {
                        continue;
                    }

                    // (TODO) Simplify?
                    TerraFloat3 Ip_q = terra_map_read_texel ( I, wx, wy );
                    Ip_q = terra_mulf3 ( &Ip_q, weight );
                    float l2 = terra_distsq ( &Ip_q, &Ip_g );
                    l2 = powf ( l2 * norm_factor, lambda );
                    Ip_q = terra_mulf3 ( &Ip_q, l2 );
                    acc = terra_addf3 ( &acc, &Ip_q );
                    acc_norm += l2;
                }
            }

            acc = terra_mulf3 ( &acc, 1.f / acc_norm );
            terra_map_write_texel ( O, x, y, &acc );
        }
    }
}

/*
    Images are downscaled using N. Weber's joint bilateral filter for
    detailed preserving image downscaling. https://dl.acm.org/citation.cfm?id=2980239
    We are downscaling an image I WxH to O W'xH'
    We first compute a guidance image I_g W'xH' by downscaling I with a box filter, then
    performing a convolution with: [1 2 1; 2 4 2; 1 2 1].
    We then compute O with a bilateral filter:
    O(p) = 1/k_p * sum_q I(q) * ((||I(q) - I_g(p)||_2 / NORM)^lambda
    q are the pixels of I which will get mapped into p
    lambda = 0.5, 1.0, have fun
    NORM = sqrt(3) for FLOAT32_RGB
*/
void terra_map_dpid_downscale ( TerraMap* O, const TerraMap* I ) {
    // Allocating guidance texture
    TerraMap I_g;
    terra_map_init ( &I_g, O->width, O->height );

    // Computing guidance texture
    terra_dpid_compute_guidance ( &I_g, I );

    // Output image
    terra_dpid_joint_bilateral_filter ( O, &I_g, I );

    // Free guidance texture
    terra_free ( I_g.data );
}
