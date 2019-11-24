#pragma once

// libc++
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>

// gl3w
#include <GL/gl3w.h>
#include <GL/GL.h>

// imgui
#include <imgui.h>

#ifndef GL_NO_ERROR
#define GL_NO_ERROR assert(glGetError() == 0)
#endif

struct Image;
using ImageHandle = std::shared_ptr<Image>;

struct Image {
    int width = -1;
    int height = -1;
    int stride = -1;
    std::unique_ptr<uint8_t[]> pixels = nullptr;
    GLuint id;
    GLenum format; // opengl *sized* format

    ~Image();
    void upload(
        int tile_x = -1,
        int tile_y = -1,
        int tile_width = -1,
        int tile_height = -1
    );

    bool valid()const;

    static ImageHandle create(const int width, const int height, const GLenum format);
};

struct GLFWwindow;

class GFXLayer {
  public:
    using OnResizeCallback = std::function<void ( int, int ) >;
    using InputHandler = std::function<void ( const ImGuiIO& ) >;

    bool  init ( int width, int height, const char* title, const OnResizeCallback& on_resize, const InputHandler& input_handler );
    int   width ();
    int   height ();
    void  process_events ();
    bool  should_quit ();
    void  swap_buffers ();
    void* get_window ();
    void  update_config();
    GLFWwindow* window_handle();

  private:
    void _resize ( int width, int height );

    GLFWwindow*      _window;
    OnResizeCallback _on_resize;
    InputHandler     _input_handler;
    int              _width;
    int              _height;
};

struct ShaderUniform {
    std::string name;
    GLint       binding;
    GLsizei     length;
    GLenum      type;

    static const char* type_to_string(const GLenum type);
};

struct Pipeline {
    Pipeline() = default;
    ~Pipeline() = default;

    void reset (
        const ImageHandle rt_color,
        const ImageHandle rt_depth,
        const char* shader_vert_src,
        const char* shader_frag_src,
        const bool enable_depth_test
    );
    void bind();
    const ShaderUniform& uniform(const char* str);

    GLuint shader_vert = -1;
    GLuint shader_frag = -1;
    GLuint program = -1;
    GLuint fbo = -1;
    GLsizei viewport[4];

    bool enable_depth_test = false;
    bool enable_wireframe = false;

private:
    GLuint _load_shader(const GLenum stage, const char* glsl);
    void _reflect_program();

    std::unordered_map<std::string, ShaderUniform> _uniforms;
};