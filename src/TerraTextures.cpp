// Header
#include <Terra.h>

// Terra private API
#include "TerraPrivate.h"

// libc
#include <string.h> // memset

/*
    Returns the number of mipmap levels for a wxh texture.
    Not required to be a power of two.
*/
int terra_mimaps_count ( uint16_t w, uint16_t h ) {
    return 0;
}

/*
    Returns the number of anisotropically scaled mipmaps for a
    wxh texture. Not required to be a power of two.
*/
int terra_ripmaps_count ( uint16_t w, uint16_t h ) {
    return 0;
}

/*
    Generates the consecutive mip level from the previous one.
*/
TerraMap terra_map_create_mip ( uint16_t w, uint16_t h, const TerraMap* src ) {
    return *src;
}

/*
    Creates the first miplevel from the input texture. It linearizes the pixel
    values and pads to 4 components, regardless of input format.
*/
TerraMap terra_map_create_mip0 ( size_t width, size_t height, size_t components, size_t depth, const void* data ) {
    TERRA_ASSERT ( depth == 1 || depth == 4 );

    size_t size = 4 * depth * width * height;

    TerraMap map;
    map.width  = ( uint16_t ) width;
    map.height = ( uint16_t ) height;
    map.data   = terra_malloc ( size );

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
        for ( size_t i = 0; i < width * height; ++i ) {

        }
    }

    return map;
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
    texture->depth = depth;
    texture->address = address;
    texture->mipmaps = nullptr;
    texture->ripmaps = nullptr;

    // Calculating additional mip levels
    size_t n_mipmaps = 1;

    /*if ( sampler & kTerraSamplerMipmaps ) {
        n_mipmaps += terra_mimaps_count ( texture );
    }

    size_t n_ripmaps = 0;

    if ( sampler & kTerraSamplerAnisotropic ) {
        n_ripmaps += terra_ripmaps_count ( texture );
    }
    */
    texture->mipmaps = ( TerraMap* ) terra_malloc ( sizeof ( TerraMap ) * n_mipmaps );
    TERRA_ZEROMEM_ARRAY ( texture->mipmaps, n_mipmaps );

    // Generating mipmaps
    texture->mipmaps[0] = terra_map_create_mip0 ( width, height, components, depth, data );

    TerraMap* last_mip = texture->mipmaps;
    uint16_t  w = texture->width >> 1;
    uint16_t  h = texture->height >> 1;

    for ( size_t i = 1; i < n_mipmaps; ++i ) {
        texture->mipmaps[i] = terra_map_create_mip ( w, h, last_mip );
        last_mip = texture->mipmaps + i;
        w >>= 1;
        h >>= 1;
    }

    // Generating ripmaps
    /*if ( n_ripmaps > 0 ) {
        texture->ripmaps = ( TerraMap* ) terra_malloc ( sizeof ( TerraMap ) * n_ripmaps );
        TERRA_ZEROMEM_ARRAY ( texture->ripmaps, n_ripmaps );
    }
    */
    return true; // ?
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

/*
    Reads a texel from a RGBA8_UNORM texture
    TODO: SSE2
*/
TerraFloat3 terra_map_read_texel ( const TerraMap* map, size_t idx ) {
    const float scale = 1.f / 255;
    uint8_t* addr = ( uint8_t* ) map->data + idx * 4;
    TerraFloat3 ret;
    ret.x = ( ( float ) * addr ) * scale;
    ret.y = ( ( float ) * addr ) * scale;
    ret.z = ( ( float ) * addr ) * scale;
    return ret;
}

/*
    Reads a texel form a RGBA32_FLOAT texture
    TODO: SSE2
*/
TerraFloat3 terra_map_read_texel_fp ( const TerraMap* map, size_t idx ) {
    return terra_f3_ptr ( ( float* ) map->data + idx * 4 );
}

// Dispatching
typedef TerraFloat3 ( *texel_read_fn ) ( const TerraMap*, size_t );
static texel_read_fn texel_read_fns[] = {
    &terra_map_read_texel,
    &terra_map_read_texel_fp
};

/*
    Pixel coordinates to pixel value.
*/
TerraFloat3 terra_texture_read_texel ( const TerraTexture* texture, int mip, uint16_t x, uint16_t y ) {
    TERRA_ASSERT ( texture->depth == 1 || texture->depth == 4 ); // Rethink the addressing otherwise

    x = terra_minu16 ( x, texture->width - 1 );
    y = terra_minu16 ( y, texture->height - 1 );
    size_t lin_idx = y * texture->width + x;
    const TerraMap* map = texture->mipmaps + mip;
    return texel_read_fns[texture->depth / 4] ( map, lin_idx ); // .. Almost (:
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
void terra_texture_texel_int ( const TerraTexture* texture, const TerraFloat2* uv, uint16_t* x, uint16_t* y ) {
    const float w = ( float ) texture->width;
    const float h = ( float ) texture->height;
    const float iw = 1.f / w;
    const float ih = 1.f / h;

    const float rx = uv->x - .5f * iw;
    const float ry = uv->y - .5f * ih;

    *x = ( uint16_t ) ( address_fns[texture->address] ( rx ) * w );
    *y = ( uint16_t ) ( address_fns[texture->address] ( ry ) * h );
}

/*
    Nearest filter
*/
TerraFloat3 terra_texture_sample_mip_nearest ( const TerraTexture* texture, int mip, const TerraFloat2* uv ) {
    const float iw = 1.f / texture->width;
    const float ih = 1.f / texture->height; // Maybe cache ?
    uint16_t x, y;
    terra_texture_texel_int ( texture, uv, &x, &y );
    return terra_texture_read_texel ( texture, mip, x, y );
}

/*
    Bilinear filter
*/
TerraFloat3 terra_texture_sample_mip_bilinear ( const TerraTexture* texture, int mip, const TerraFloat2* _uv ) {
    TerraFloat2 uv = terra_f2_set ( _uv->x * texture->width, _uv->y * texture->height );

    uint16_t x1, y1;
    terra_texture_texel_int ( texture, _uv, &x1, &y1 );

    uint16_t x2 = x1 + 1;
    uint16_t y2 = y1;

    uint16_t x3 = x1;
    uint16_t y3 = y1 + 1;

    uint16_t x4 = x1 + 1;
    uint16_t y4 = y1 + 1;

    TerraFloat3 t1 = terra_texture_read_texel ( texture, mip, x1, y1 );
    TerraFloat3 t2 = terra_texture_read_texel ( texture, mip, x2, y2 );
    TerraFloat3 t3 = terra_texture_read_texel ( texture, mip, x3, y3 );
    TerraFloat3 t4 = terra_texture_read_texel ( texture, mip, x4, y4 );

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
typedef TerraFloat3 ( *texture_sample_fn ) ( const TerraTexture* texture, int mip, const TerraFloat2* uv );
static texture_sample_fn sample_fns[] = {
    terra_texture_sample_mip_nearest,
    terra_texture_sample_mip_bilinear
};

/*
    Samples a specific miplevel at <u v>
*/
TerraFloat3 terra_texture_sample_mip ( const TerraTexture* texture, int mip, const TerraFloat2* uv ) {
    return sample_fns[texture->sampler & 0x1] ( texture, mip, uv );
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