// Header
#include <Terra.h>

// Terra private API
#include "TerraPrivate.h"

// libc
#include <string.h> // memset

#include "../satellite/src/stb_image_write.h"

// TODO: Might want to cache these ?
int terra_mimaps_count ( const TerraTexture* texture ) {
    return 0;
}

int terra_ripmaps_count ( const TerraTexture* texture ) {
    return 0;
}

TerraMap terra_map_create_mip ( uint16_t w, uint16_t h, const TerraMap* src ) {
    return *src;
}

// Padding to 4 components, subsequent mips just downscale
TerraMap terra_map_create_mip0 ( size_t width, size_t height, size_t components, size_t depth, const void* data ) {
    TERRA_ASSERT ( depth == 1 || depth == 4 );

    size_t size = 4 * depth * width * height;

    TerraMap map;
    map.width  = ( uint16_t ) width;
    map.height = ( uint16_t ) height;
    map.data.p = ( uint8_t* ) terra_malloc ( size );

    if ( depth == 1 ) {
        uint8_t*       w = map.data.p;
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

    stbi_write_png ( "example.png", map.width, map.height, 4, map.data.p, map.width );

    return map;
}

void terra_map_destroy ( TerraMap* map ) {
    if ( map->data.p != nullptr ) {
        terra_free ( map->data.p );
    }

    TERRA_ZEROMEM ( map );
}

bool terra_texture_create_any ( TerraTexture* texture, const void* data, size_t width, size_t height, size_t depth, size_t components, size_t sampler, TerraTextureAddress address ) {
    TERRA_ASSERT ( texture && data );
    TERRA_ASSERT ( width > 0 && height > 0 && components > 0 );
    TERRA_ASSERT ( width < ( uint16_t ) - 1 && height < ( uint16_t ) - 1 );

    // TODO: Convert texture coordinates at finalize()

    // Initializing
    if ( sampler & kTerraSamplerAnisotropic ) {
        sampler |= kTerraSamplerTrilinear;
    }

    texture->width = ( uint16_t ) width;
    texture->height = ( uint16_t ) height;
    texture->sampler = sampler;
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

bool terra_texture_create ( TerraTexture* texture, const uint8_t* data, size_t width, size_t height, size_t components, size_t sampler, TerraTextureAddress address ) {
    return terra_texture_create_any ( texture, data, width, height, 1, components, sampler, address );
}

bool terra_texture_create_fp ( TerraTexture* texture, const float* data, size_t width, size_t height, size_t components, size_t sampler, TerraTextureAddress address ) {
    return terra_texture_create_any ( texture, data, width, height, 4, components, sampler, address );
}

void terra_texture_destroy ( TerraTexture* texture ) {
    if ( !texture ) {
        return;
    }

    if ( texture->sampler & kTerraSamplerTrilinear ) {
        size_t n_mipmaps = 1 + terra_mimaps_count ( texture );

        for ( size_t i = 1; i < n_mipmaps; ++i ) {
            terra_map_destroy ( texture->mipmaps + i );
        }
    }

    TERRA_ASSERT ( texture->mipmaps );
    terra_map_destroy ( texture->mipmaps );
    free ( texture->mipmaps ); // mip0 should be preset

    if ( texture->ripmaps ) {
        size_t n_ripmaps = terra_ripmaps_count ( texture );

        for ( size_t i = 0; i < n_ripmaps; ++i ) {
            terra_map_destroy ( texture->ripmaps + i );
        }

        free ( texture->ripmaps );
    }
}

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

// TODO: sse2 this
TerraFloat3 terra_map_read_texel ( const TerraMap* map, size_t idx ) {
    const float scale = 1.f / 255;
    uint8_t* addr = map->data.p + idx * 4;
    TerraFloat3 ret;
    ret.x = ( ( float ) * addr ) * scale;
    ret.y = ( ( float ) * addr ) * scale;
    ret.z = ( ( float ) * addr ) * scale;
    return ret;
}

TerraFloat3 terra_map_read_texel_fp ( const TerraMap* map, size_t idx ) {
    return terra_f3_ptr ( map->data.fp + idx * 4 );
}

typedef TerraFloat3 ( *texel_read_fn ) ( const TerraMap*, size_t );
static texel_read_fn texel_read_fns[] = {
    &terra_map_read_texel,
    &terra_map_read_texel_fp
};

TerraFloat3 terra_texture_read_texel ( const TerraTexture* texture, int mip, uint16_t x, uint16_t y ) {
    TERRA_ASSERT ( texture->depth == 1 || texture->depth == 4 ); // Rethink the addressing otherwise

    x = terra_mini ( x, texture->width - 1 );
    y = terra_mini ( y, texture->height - 1 );
    size_t lin_idx = y * texture->width + x;
    const TerraMap* map = texture->mipmaps + mip;
    return texel_read_fns[texture->depth / 4] ( map, lin_idx ); // .. Almost (:
}

float terra_texture_address_wrap ( float v ) {
    return v < 0.f ? fmod ( -v, 1.f ) : ( v > 1.f ? fmod ( v, 1.f ) : v );
}

float terra_texture_address_mirror ( float v ) {
    return v < 0.f ? 1.f - fmod ( -v, 1.f ) : ( v > 1.f ? 1.f - fmod ( v, 1.f ) : v );
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

TerraFloat3 terra_texture_sample_mip_nearest ( const TerraTexture* texture, int mip, const TerraFloat2* uv ) {
    const float iw = 1.f / texture->width;
    const float ih = 1.f / texture->height; // Maybe cache ?
    uint16_t x, y;
    terra_texture_texel_int ( texture, uv, &x, &y );
    return terra_texture_read_texel ( texture, mip, x, y );
}

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

typedef TerraFloat3 ( *texture_sample_fn ) ( const TerraTexture* texture, int mip, const TerraFloat2* uv );
static texture_sample_fn sample_fns[] = {
    terra_texture_sample_mip_nearest,
    terra_texture_sample_mip_bilinear
};

TerraFloat3 terra_texture_sample_mip ( const TerraTexture* texture, int mip, const TerraFloat2* uv ) {
    return sample_fns[texture->sampler & 0x1] ( texture, mip, uv );
}

TERRA_TEXTURE_SAMPLER ( mip0 ) {
    return terra_texture_sample_mip ( ( TerraTexture* ) state, 0, ( const TerraFloat2* ) addr );
}

TERRA_TEXTURE_SAMPLER ( mipmaps ) {
    return terra_f3_set ( 1.f, 1.f, 1.f );
}

TERRA_TEXTURE_SAMPLER ( anisotropic ) {
    return terra_f3_set ( 1.f, 1.f, 1.f );
}

TERRA_TEXTURE_SAMPLER ( spherical ) {
    return terra_f3_set ( 1.f, 1.f, 1.f );
}