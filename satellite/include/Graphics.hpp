#pragma once

// C++ STL
#include <cstdint>
#include <functional>

// imgui
#include <imgui.h>

// To avoid having TerraFramebuffers going around
// and also used by a couple of other classes for convenience
struct TextureData {
    float* data       = nullptr;
    int    width      = -1;
    int    height     = -1;
    int    components = -1;
};

struct GLFWwindow;

class GFXLayer {
  public:
    using OnResizeCallback = std::function<void ( int, int ) >;
    using InputHandler = std::function<void ( const ImGuiIO& ) >;

    bool init ( int width, int height, const char* title, const OnResizeCallback& on_resize, const InputHandler& input_handler );
    void resize ( int width, int height );
    int width ();
    int height ();
    void process_events ();
    bool should_quit ();
    void swap_buffers ();
    void* get_window ();

  private:
    GLFWwindow*      _window;
    OnResizeCallback _on_resize;
    InputHandler     _input_handler;
    int              _width;
    int              _height;
};