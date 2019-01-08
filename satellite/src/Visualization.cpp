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

// Terra
#include <TerraProfile.h>
#include <TerraPresets.h>

// OpenGL functions
#include <GL/gl3w.h>

// stb
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

namespace {
    const ImVec4 IM_WHITE ( 1.f, 1.f, 1.f, 1.f );
    const ImVec4 IM_BLACK ( 0.15f, 0.15f, 0.15f, 1.0f );
    const ImVec4 IM_TRANSPARENT ( 0.f, 0.f, 0.f, 0.f );

    enum class ImAlign {
        TopLeft,
        BottomLeft,
        TopRight,
        Middle,
        TopMiddle,
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

            case ImAlign::TopMiddle: {
                text_pos = ImVec2 ( w / 2, padding );
                text_pos.x -= text_size.x * 0.5f;
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
}

void Visualizer::init ( GFXLayer* gfx ) {
    _gfx              = gfx;
    _gl_format        = -1;
    _gl_texture       = -1;
    _hide_info        = false;
    _info.spp         = -1;
    _info.accelerator = "n/a";
    _info.sampling    = "n/a";
    _texture.width    = 0;
    _texture.height   = 0;
    terra_clock_init();
}

void Visualizer::create_texture ( int width, int height, int gl_format, void* data ) {
    glGenTextures ( 1, &_gl_texture );
    glBindTexture ( GL_TEXTURE_2D, _gl_texture );
    // When the window is resized and the old texture is still being shown, use nearest filter
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexImage2D ( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, gl_format, GL_FLOAT, data );

    if ( !glIsTexture ( _gl_texture ) ) {
        Log::error ( STR ( "Failed to create OpenGL texture, check debug messages." ) );
    }
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
        create_texture ( texture.width, texture.height, gl_format, texture.data );
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
        create_texture ( texture.width, texture.height, gl_format, texture.data );
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
            _texture.data[ ( i + y ) * texture.width * texture.components + x * texture.components + j ] = texture.data[ ( y + i ) * texture.width * texture.components + x * texture.components + j];
        }
    }

    _gl_format = gl_format;
}

void Visualizer::save_to_file ( const char* _path ) {
    static_assert ( is_same<remove_pointer<decltype ( TextureData::data ) >::type, float>::value, "Code needs to be updated for non-floating point textures." );
    char path[256];
    strcpy ( path, _path );

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

    if ( ext == path ) {
        Log::warning ( FMT ( "No file extension specified, png assumed by default." ) );
        strcpy ( path + path_len, ".png" );
        ext = path + path_len + 1;
    } else {
        ++ext; // Skipping `.`
    }

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

void Visualizer::toggle_info() {
    _hide_info = !_hide_info;
}

void Visualizer::draw() {
    const float width  = ( float ) _gfx->width();
    const float height = ( float ) _gfx->height();
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
        const char* msg = "          render something!\n"
                          "press ` (backtick) to toggle console\n"
                          "  enter help for a list of commands\n";
        im_text_aligned ( ImAlign::TopMiddle, msg, IM_WHITE, IM_TRANSPARENT, ImVec2 ( 0.f, 50.f ) );
    }

    if ( ! _info.scene.empty() ) {
        if ( !_hide_info ) {
            constexpr int INFO_BUF_LEN = 1024;
            char info_buf[INFO_BUF_LEN];
            string spp = _info.spp != -1 ? to_string ( _info.spp ) : "n/a";
            snprintf ( info_buf, INFO_BUF_LEN, "%s\nspp: %s\naccelerator: %s\nsampling: %s\nresolution: %dx%d", _info.scene.c_str(), spp.c_str(), _info.accelerator.c_str(), _info.sampling.c_str(), _texture.width, _texture.height );
            im_text_aligned ( ImAlign::TopLeft, info_buf, IM_WHITE, ImVec4 ( 0.f, 0.f, 0.f, 0.5f ) );
        }
    }

#ifdef TERRA_PROFILE
    size_t offset = 0;

    for ( auto& stats : _stats ) {
        constexpr int STATS_BUF_LEN = 1024;
        char stats_buf[STATS_BUF_LEN];
        TerraProfileStats data = stats.data;

        float       unit_factor = 1.;
        const char* unit_name = "us";

        if ( stats.type == TIME && stats.data.n > 0 ) {
            if ( stats.target == TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION ) {
                fprintf ( stderr, "AVERAGE %e\n", data.avg );
            }

            data.avg = terra_clock_to_us ( ( int64_t ) ( data.avg * unit_factor ) );
            data.var = terra_clock_to_us ( ( int64_t ) ( data.var * unit_factor ) );
            data.min = terra_clock_to_us ( ( int64_t ) ( data.min * unit_factor ) );
            data.max = terra_clock_to_us ( ( int64_t ) ( data.max * unit_factor ) );
            data.sum = terra_clock_to_us ( ( int64_t ) ( data.sum * unit_factor ) );
        }

        snprintf ( stats_buf, STATS_BUF_LEN, "%s(%s)\n avg: %e\n var: %e\n min: %e\n max: %e\n sum: %e\n count: %.0f\n",
                   unit_name, stats.name.c_str(), data.avg, data.var, data.min, data.max, data.sum, data.n );
        im_text_aligned ( ImAlign::TopRight, stats_buf, IM_WHITE, ImVec4 ( 0.f, 0.f, 0.f, 0.5f ), ImVec2 ( 0, offset ) );
        offset += 120;
    }

#endif
    End();
    PopStyleVar ( 2 );
    PopStyleColor ( 1 );
}

Visualizer::Info& Visualizer::info() {
    return _info;
}

std::vector<Visualizer::Stats>& Visualizer::stats() {
    return _stats;
}

void Visualizer::add_stats_tracker ( size_t session, size_t target, const char* name ) {
#ifdef TERRA_PROFILE
    TerraProfileStats data = terra_profile_target_stats_get ( session, target );
    Stats stats;
    stats.data = data;
    stats.name = std::string ( name );
    stats.session = session;
    stats.target = target;
    stats.type = terra_profile_target_type_get ( session, target );
    _stats.push_back ( stats );
#endif
}

void Visualizer::remove_stats_tracker ( const char* name ) {
    for ( auto it = _stats.begin(); it != _stats.end(); ++it ) {
        if ( it->name.compare ( name ) == 0 ) {
            _stats.erase ( it );
            break;
        }
    }
}

void Visualizer::remove_all_stats_trackers() {
    _stats.clear();
}

void Visualizer::update_stats() {
#ifdef TERRA_PROFILE

    for ( auto& stats : _stats ) {
        stats.data = terra_profile_target_stats_get ( stats.session, stats.target );
    }

#endif
}