#pragma once

// C++ STL
#include <cstdint>
#include <functional>

// imgui
#include <imgui.h>

// Used to avoid having TerraFramebuffers going around
// and used also by a couple of other classes for convenience
struct TextureData {
    float* data       = nullptr;
    int    width      = -1;
    int    height     = -1;
    int    components = -1;
};

// One reason to have this opaque interface is to avoid including system header libraries in other header files
using GFXLayer = void*;
using OnResizeCallback = std::function<void ( int, int ) >;
using InputHandler = std::function<void ( const ImGuiIO& ) >;   // not a callback! called on every frame, should poll on ImGuiIO

GFXLayer gfx_init ( int width, int height, const char* title, const OnResizeCallback& on_resize, const InputHandler& input_handler );
void     gfx_resize ( GFXLayer gfx, int width, int height );
int      gfx_width ( GFXLayer gfx );
int      gfx_height ( GFXLayer gfx );
void     gfx_process_events ( GFXLayer gfx );
bool     gfx_should_quit ( GFXLayer gfx );
void     gfx_swap_buffers ( GFXLayer gfx );
void*    gfx_get_window ( GFXLayer gfx );