#pragma once

// C++ STL
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

// gl3w
#include <GL/gl3w.h>
#include <GL/GL.h>

// imgui
#include <imgui.h>

#define GL_NO_ERROR assert(glGetError() == 0)

#define OPENGL_RESOURCE_COPY_VALID_ID(other_res) assert(res != INVALID_ID); res = other_res;
#define DEFINE_OPENGL_RESOURCE(name, dtor)\
struct name {\
    static const GLuint INVALID_ID = (GLuint)-1;\
    virtual ~name() {\
        dtor;\
    }\
\
    name(GLuint res = INVALID_ID) : res(res) { assert(res != INVALID_ID); }\
\
    name(name&& other) { OPENGL_RESOURCE_COPY_VALID_ID(other.res) other.res = INVALID_ID; }\
    name(name& other) { OPENGL_RESOURCE_COPY_VALID_ID(other.res) other.res = INVALID_ID; }\
    name& operator=(name&& other) { OPENGL_RESOURCE_COPY_VALID_ID(other.res) other.res = INVALID_ID; return *this; }\
    name& operator=(name& other) { OPENGL_RESOURCE_COPY_VALID_ID(other.res) other.res = INVALID_ID; return *this; }\
    name& operator=(GLuint other_res) { OPENGL_RESOURCE_COPY_VALID_ID(other_res) return *this; }\
\
    GLuint* operator&() { return &res; }\
    const GLuint* operator&() const { return &res; }\
    operator GLuint& () { return res; }\
    operator const GLuint& () const { return res; }\
\
protected:\
    GLuint res = -1;\
};

DEFINE_OPENGL_RESOURCE(OpenGLShader, if (glIsShader(res)) glDeleteShader(res); )
DEFINE_OPENGL_RESOURCE(OpenGLBuffer, if (glIsBuffer(res)) glDeleteBuffers(1, &res); )
DEFINE_OPENGL_RESOURCE(OpenGLTexture, if (glIsTexture(res)) glDeleteTextures(1, &res); )
DEFINE_OPENGL_RESOURCE(OpenGLFramebuffer, if (glIsFramebuffer(res)) glDeleteFramebuffers(1, &res); )
DEFINE_OPENGL_RESOURCE(OpenGLVertexArray, if (glIsVertexArray(res)) glDeleteVertexArrays(1, &res); )
DEFINE_OPENGL_RESOURCE(OpenGLProgram, if (glIsShader(res)) glDeleteShader(res); )

using ImageID = GLuint;

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

    bool  init ( int width, int height, const char* title, const OnResizeCallback& on_resize, const InputHandler& input_handler );
    int   width ();
    int   height ();
    void  process_events ();
    bool  should_quit ();
    void  swap_buffers ();
    void* get_window ();
    void  update_config();

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
};

struct Pipeline {
    Pipeline() = default;
    ~Pipeline() = default;

    void reset (
        GLuint rt_color,
        const char* shader_vert_src,
        const char* shader_frag_src,
        const int   width,
        const int   height
    );
    void bind();

    GLuint shader_vert;
    GLuint shader_frag;
    GLuint program;

    GLuint fbo;
    GLuint rt_color;
    GLuint rt_depth;

private:
    GLuint _load_shader(const GLenum stage, const char* glsl);
    void _reflect_program();

    std::unordered_map<std::string, ShaderUniform> _uniforms;
  
};