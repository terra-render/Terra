// Header
#include <Visualization.hpp>

// C++ STL
#include <fstream>
#include <algorithm>

// Satellite
#include <Logging.hpp>

// Imgui
#include <imgui.h>
#include <examples/opengl3_example/imgui_impl_glfw_gl3.h>

// OpenGL functions
#include <GL/gl3w.h>

// stb
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Terra
#include <TerraPrivate.h>

using namespace std;

namespace {
    const ImVec4 IM_WHITE ( 1.f, 1.f, 1.f, 1.f );
    const ImVec4 IM_BLACK ( 0.15f, 0.15f, 0.15f, 1.0f );
    const ImVec4 IM_TRANSPARENT ( 0.f, 0.f, 0.f, 0.f );

    enum class ImAlign {
        TopLeft,
        BottomLeft,
        TopRight,
        Middle
    };

    // with respect to current window
    void im_text_aligned ( const ImAlign& align, const char* msg, const ImVec4& color, const ImVec4& bg_color = IM_TRANSPARENT, const ImVec2& off = ImVec2 ( 0.f, 0.f ) ) {
        using namespace ImGui;
        ImVec2 text_size = CalcTextSize ( msg );
        ImVec2 text_pos;
        const float w = GetWindowWidth();
        const float h = GetWindowHeight();
        const float padding = 5.f;

        switch ( align ) {
            case ImAlign::TopLeft: {
                text_pos = ImVec2 ( padding, padding );
                break;
            }

            case ImAlign::BottomLeft: {
                text_pos = ImVec2 ( padding, h - padding );
                text_pos.y -= text_size.y;
                break;
            }

            case ImAlign::TopRight: {
                text_pos = ImVec2 ( w - padding, padding );
                text_pos.x -= text_size.x;
                break;
            }

            case ImAlign::Middle: {
                text_pos = ImVec2 ( w / 2, h / 2 );
                text_pos.x -= text_size.x * 0.5f;
                text_pos.y -= text_size.y * 0.5f;
                break;
            }

            default: {
                Log::error ( STR ( "Invalid alignment" ) );
                return;
            }
        }

        text_pos.x += off.x;
        text_pos.y += off.y;

        if ( bg_color.w > 0.f ) {
            ImVec2 rect_tl ( text_pos );
            ImVec2 rect_br ( text_pos.x + text_size.x, text_pos.y + text_size.y );
            rect_tl.x -= padding;
            rect_tl.y -= padding;
            rect_br.x += padding;
            rect_br.y += padding;
            GetWindowDrawList()->AddRectFilled ( rect_tl, rect_br, ImColor ( bg_color ) );
        }

        SetCursorPos ( text_pos );
        TextColored ( ImVec4 ( 1.f, 1.f, 1.f, 1.f ), msg );
    }

    // Implemented at the bottom of the file
    void debug_view_samplers ( bool );
    void debug_view_aa       ( bool );
}

void Visualizer::init ( GFXLayer gfx ) {
    _gfx              = gfx;
    _gl_format        = -1;
    _gl_texture       = -1;
    _hide_info        = false;
    _texture.width    = 0;
    _texture.height   = 0;
}

void Visualizer::set_texture_data ( const TextureData& texture ) {
    static_assert ( is_same<remove_pointer<decltype ( TextureData::data ) >::type, float>::value, "Code needs to be updated for non-floating point textures." );

    if ( texture.data == nullptr ) {
        Log::error ( STR ( "No texture data present." ) );
        return;
    }

    // Finding format given components
    int gl_format;

    if ( texture.components == 3 ) {
        gl_format = GL_RGB;
    } else if ( texture.components == 4 ) {
        gl_format = GL_RGBA;
    } else {
        Log::error ( FMT ( "Invalid texture components %d", texture.components ) );
        return;
    }

    // Need to generate new texture
    bool create_gl_texture = _texture.data == nullptr;

    if ( _texture.data != nullptr && ( _texture.width != texture.width || _texture.height != texture.height || texture.components != _texture.components ) ) {
        if ( glIsTexture ( _gl_texture ) ) {
            glDeleteTextures ( 1, &_gl_texture );
        } else {
            Log::error ( STR ( "Unexpected behavior" ) );
        }

        create_gl_texture = true;
    }

    // Creating OpenGL texture, which will contain the render results
    if ( create_gl_texture ) {
        glGenTextures ( 1, &_gl_texture );
        glBindTexture ( GL_TEXTURE_2D, _gl_texture );
        // When the window is resized and the old texture is still being shown, use nearest filter
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        glTexImage2D ( GL_TEXTURE_2D, 0, GL_RGB, texture.width, texture.height, 0, gl_format, GL_FLOAT, texture.data );

        if ( !glIsTexture ( _gl_texture ) ) {
            Log::error ( STR ( "Failed to create OpenGL texture, check debug messages." ) );
            return;
        }
    }
    // If the current texture is valid, just uploading the new data
    else {
        glBindTexture ( GL_TEXTURE_2D, _gl_texture );
        glTexSubImage2D ( GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, gl_format, GL_FLOAT, texture.data );
    }

    // Creating local copy
    _texture.width = texture.width;
    _texture.height = texture.height;
    _texture.components = texture.components;
    int n_values = texture.width * texture.height * texture.components;

    // Resizing if necessary
    if ( create_gl_texture || _texture.data != nullptr ) {
        delete[] _texture.data;
        _texture.data = nullptr;
    }

    if ( _texture.data == nullptr ) {
        _texture.data = new float[n_values];
    }

    memcpy ( _texture.data, texture.data, sizeof ( float ) * n_values );
    _gl_format = gl_format;
}

void Visualizer::update_tile ( const TextureData& texture, size_t x, size_t y, size_t w, size_t h ) {
    static_assert ( is_same<remove_pointer<decltype ( TextureData::data ) >::type, float>::value, "Code needs to be updated for non-floating point textures." );

    if ( texture.data == nullptr ) {
        Log::error ( STR ( "No texture data present." ) );
        return;
    }

    // Finding format given components
    int gl_format;

    if ( texture.components == 3 ) {
        gl_format = GL_RGB;
    } else if ( texture.components == 4 ) {
        gl_format = GL_RGBA;
    } else {
        Log::error ( FMT ( "Invalid texture components %d", texture.components ) );
        return;
    }

    // Need to generate new texture
    bool create_gl_texture = _texture.data == nullptr;

    if ( _texture.data != nullptr && ( _texture.width != texture.width || _texture.height != texture.height || texture.components != _texture.components ) ) {
        if ( glIsTexture ( _gl_texture ) ) {
            glDeleteTextures ( 1, &_gl_texture );
        } else {
            Log::error ( STR ( "Unexpected behavior" ) );
        }

        create_gl_texture = true;
    }

    // Creating OpenGL texture, which will contain the render results
    if ( create_gl_texture ) {
        glGenTextures ( 1, &_gl_texture );
        glBindTexture ( GL_TEXTURE_2D, _gl_texture );
        // When the window is resized and the old texture is still being shown, use nearest filter
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        glTexImage2D ( GL_TEXTURE_2D, 0, GL_RGB, texture.width, texture.height, 0, gl_format, GL_FLOAT, texture.data );

        if ( !glIsTexture ( _gl_texture ) ) {
            Log::error ( STR ( "Failed to create OpenGL texture, check debug messages." ) );
            return;
        }
    }

    vector<float> buffer ( w * h * texture.components, 0 );

    for ( size_t i = 0; i < h; ++i ) {
        for ( size_t j = 0; j < w * texture.components; ++j ) {
            buffer[i * w * texture.components + j] =  texture.data[ ( y + i ) * texture.width * texture.components + x * texture.components + j];
        }
    }

    glBindTexture ( GL_TEXTURE_2D, _gl_texture );
    glTexSubImage2D ( GL_TEXTURE_2D, 0, x, y, w, h, gl_format, GL_FLOAT, buffer.data() );
    // Creating local copy
    _texture.width = texture.width;
    _texture.height = texture.height;
    _texture.components = texture.components;
    int n_values = texture.width * texture.height * texture.components;

    // Resizing if necessary
    if ( create_gl_texture ) {
        delete[] _texture.data;
        _texture.data = nullptr;
    }

    if ( _texture.data == nullptr ) {
        _texture.data = new float[n_values];
    }

    for ( size_t i = 0; i < h; ++i ) {
        for ( size_t j = 0; j < w * texture.components; ++j ) {
            _texture.data[i * w * texture.components + j] = texture.data[ ( y + i ) * texture.width * texture.components + x * texture.components + j];
        }
    }

    _gl_format = gl_format;
}

void Visualizer::save_to_file ( const char* path ) {
    static_assert ( is_same<remove_pointer<decltype ( TextureData::data ) >::type, float>::value, "Code needs to be updated for non-floating point textures." );

    if ( _texture.data == nullptr ) {
        Log::error ( STR ( "Nothing to export" ) );
        return;
    }

    if ( path == nullptr ) {
        Log::error ( STR ( "Invalid path" ) );
        return;
    }

    int path_len = ( int ) strlen ( path );

    if ( path_len < 3 ) {
        Log::error ( STR ( "Invalid path" ) );
        return;
    }

    const char* ext = path + path_len - 1;

    while ( ext != path && *ext != '.' ) {
        --ext;
    }

    ++ext; // Skipping `.`
    bool is_png = strcmp ( ext, "png" ) == 0;
    bool is_jpg = strcmp ( ext, "jpg" ) == 0 || strcmp ( ext, "jpeg" ) == 0;
    int ret = -1;

    if ( is_png || is_jpg ) {
        int n_values = _texture.width * _texture.height * _texture.components;
        vector<uint8_t> ldr ( n_values );
        bool overflow = false;
        auto clamp = [] ( float v ) {
            return min ( max ( v, 0.f ), 1.f );
        };

        for ( int i = 0; i < n_values; ++i ) {
            overflow = overflow || _texture.data[i] > 1.f || _texture.data[i] < 0.f;
            ldr[i] = ( uint8_t ) ( clamp ( _texture.data[i] ) * 255.f );
        }

        if ( overflow ) {
            Log::warning ( FMT ( "Overflow when converting floating point to byte texture %s", path ) );
        }

        if ( is_png ) {
            ret = stbi_write_png ( path, _texture.width, _texture.height, _texture.components, ldr.data(), _texture.width * _texture.components );
        } else if ( is_jpg ) {
            ret = stbi_write_jpg ( path, _texture.width, _texture.height, _texture.components, ldr.data(), 100 );
        }
    } else if ( strcmp ( ext, "hdr" ) == 0 ) {
        ret = stbi_write_hdr ( path, _texture.width, _texture.height, _texture.components, _texture.data );
    }

    if ( ret == 0 ) {
        Log::error ( FMT ( "Failed to save image %s", path ) );
    } else {
        Log::info ( FMT ( "Saved %s", path ) );
    }
}

void Visualizer::toggle_debug_view ( const char* name ) {
    if ( strcmp ( name, "samplers" ) == 0 ) {
        auto res = find ( _dbg_views.begin(), _dbg_views.end(), debug_view_samplers );

        if ( res == _dbg_views.end() ) {
            debug_view_samplers ( true );
            _dbg_views.push_back ( debug_view_samplers );
        } else {
            _dbg_views.erase ( _dbg_views.begin() + distance ( _dbg_views.begin(), res ) );
        }
    }

    else if ( strcmp ( name, "aa" ) == 0 ) {
        auto res = find ( _dbg_views.begin(), _dbg_views.end(), debug_view_aa );

        if ( res == _dbg_views.end() ) {
            debug_view_aa ( true );
            _dbg_views.push_back ( debug_view_aa );
        } else {
            _dbg_views.erase ( _dbg_views.begin() + distance ( _dbg_views.begin(), res ) );
        }
    }

    else {
        assert ( false );
    }
}

void Visualizer::draw() {
    const float width  = ( float ) gfx_width ( _gfx );
    const float height = ( float ) gfx_height ( _gfx );
    using namespace ImGui;
    // No window decorations / padding / borders
    int style = ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoBringToFrontOnFocus;
    PushStyleVar ( ImGuiStyleVar_WindowRounding, 0.f );
    PushStyleVar ( ImGuiStyleVar_WindowPadding, ImVec2 ( 0.f, 0.f ) );
    PushStyleColor ( ImGuiCol_WindowBg, ImVec4 ( 0.15f, 0.15f, 0.15f, 1.f ) );
    ImVec2 screen_size = ImVec2 ( width, height );
    SetNextWindowPos ( ImVec2 ( 0.f, 0.f ) );
    SetNextWindowSize ( screen_size );
    Begin ( "moo", nullptr, style );

    if ( _texture.data != nullptr ) {
        Image ( ( ImTextureID& ) _gl_texture, screen_size );
    } else {
        const char* msg = "        render something!\nPress ` ( backtick ) to toggle console";
        im_text_aligned ( ImAlign::Middle, msg, IM_WHITE, IM_TRANSPARENT, ImVec2 ( 0.f, -20.f ) );
    }

    End();
    PopStyleVar ( 2 );
    PopStyleColor ( 1 );

    for ( const auto& view : _dbg_views ) {
        view ( false );
    }
}

//
// Debug views
//
namespace {
    void debug_view_samplers ( bool init ) {
        using namespace ImGui;
        static bool open = true;

        constexpr size_t n_samples = 256;
        static TerraFloat2 patterns[kTerraAAPatternCount][n_samples];
        static const char* names[] = {
            "random", "stratified", "halton", "hammersley", "grid", "poisson"
        };

        if ( init ) {
            TerraSamplerRandom rng;
            terra_sampler_random_init ( &rng );

            terra_pattern_random ( patterns[0], n_samples, &rng );
            terra_pattern_stratified ( patterns[1], n_samples, &rng );
            terra_pattern_halton ( patterns[2], n_samples, &rng );
            terra_pattern_hammersley ( patterns[3], n_samples, &rng );
            terra_pattern_grid ( patterns[4], n_samples, &rng );
            terra_pattern_poisson ( patterns[5], n_samples, &rng );

            for ( size_t i = 0; i < kTerraAAPatternCount; ++i ) {
                Log::info ( FMT ( "Pattern %s" ), names[i] );

                for ( size_t s = 0; s < n_samples; ++s ) {
                    Log::info ( FMT ( "Sample %.4f %.4f", patterns[i][s].x, patterns[i][s].y ) );
                }
            }
        }

        auto draw_pattern = [n_samples ] ( int i ) {
            const float size = 200.f;
            const float w = GetWindowWidth();
            const int n_col = ( int ) w / size;
            int row = i / n_col;
            int col = i % n_col;
            auto ori = GetWindowPos();
            ImVec2 loc ( ori.x +  size * col, ori.y + ( size + 50.f ) * row );
            GetWindowDrawList()->AddRectFilled ( loc,  ImVec2 ( loc.x + size, loc.y + size ), ImColor ( 255, 255, 255 ) );

            for ( size_t s = 0; s < n_samples; ++s ) {
                const float point_size = .5f * ( ( float ) n_samples  / size );
                TerraFloat2 sample = patterns[i][s];
                GetWindowDrawList()->AddCircleFilled ( ImVec2 ( loc.x + sample.x * size, loc.y + sample.y * size ), point_size, ImColor ( 0, 0, 0 ) );
            }

            GetWindowDrawList()->AddText ( ImVec2 ( loc.x + size * .5f, loc.y + size ), ImColor ( 255, 255, 255 ), names[i] );
        };

        SetNextWindowPos ( ImVec2 ( 0, 0 ) );
        SetNextWindowSize ( ImVec2 ( 1000, 280 ) );
        Begin ( "Samplers" );

        draw_pattern ( 0 );
        draw_pattern ( 1 );
        draw_pattern ( 2 );
        draw_pattern ( 3 );
        draw_pattern ( 4 );

        End();
    }

    void debug_view_aa ( bool init ) {
        using namespace ImGui;

        Begin ( "Antialiasing" );


        End();
    }
}