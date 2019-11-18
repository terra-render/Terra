// Header
#include <Graphics.hpp>

// glew
#include <GLFW/glfw3.h>

// Satellite
#include <Logging.hpp>
#include <Config.hpp>

// imgui
#include <examples/opengl3_example/imgui_impl_glfw_gl3.h>

// libc++
#include <memory>

#ifdef _WIN32

// Skipping notification, reporting everything else
void GLAPIENTRY opengl_debug_callback ( GLenum source, GLenum type, GLuint id, GLenum severity,
                                        GLsizei length, const char* message, const void* gfx_ptr ) {
    GFXLayer* gfx = ( GFXLayer* ) gfx_ptr;

    if ( severity == GL_DEBUG_SEVERITY_NOTIFICATION ) {
        return;
    }

    if ( severity == GL_DEBUG_SEVERITY_HIGH ) {
        //Log::error ( "OpenGL Error: %s\n", message );
        fprintf(stderr, "OpenGL Error: %s\n", message);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM ) {
        //Log::warning ( "OpenGL Warning: %s\n", message );
        fprintf(stderr, "OpenGL Warning: %s\n", message);
    } 
}

LRESULT CALLBACK window_callback ( HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam ) {
    return DefWindowProcA ( hwnd, umsg, wparam, lparam );
}

static void glfw_error_callback ( int error, const char* description ) {
    fprintf ( stderr, "Error %d: %s\n", error, description );
}

bool GFXLayer::init ( int width, int height, const char* title, const OnResizeCallback& on_resize, const InputHandler& input_handler ) {
    glfwSetErrorCallback ( glfw_error_callback );
    bool enable_opengl_debug = false;
#ifdef _DEBUG
    enable_opengl_debug = true;
#endif

    if ( !glfwInit() ) {
        Log::error ( STR ( "Failed to initialize GLFW" ) );
        return false;
    }

    glfwWindowHint ( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint ( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint ( GLFW_RESIZABLE, false );
    glfwWindowHint ( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

    if ( enable_opengl_debug ) {
        glfwWindowHint ( GLFW_OPENGL_DEBUG_CONTEXT, 1 );
    }

#if __APPLE__
    glfwWindowHint ( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
#endif
    _window = glfwCreateWindow ( width, height, title, nullptr, nullptr );
    glfwSetWindowPos ( _window, 100, 100 );
    glfwMakeContextCurrent ( _window );
    glfwSwapInterval ( 1 ); // vsync
    gl3wInit(); // opengl function pointers

    // Opengl error messages
    if ( enable_opengl_debug ) {
        glEnable ( GL_DEBUG_OUTPUT );
    }

    glDebugMessageCallback ( opengl_debug_callback, this );
    // Initializing GFXLayer
    // TODO hook window resize listener
    _on_resize      = on_resize;
    _input_handler  = input_handler;
    _width          = width;
    _height         = height;
    glfwSetWindowUserPointer ( _window, this );
    // Setting initial
    glViewport ( 0, 0, width, height );
    return true;
}

void GFXLayer::_resize ( int width, int height ) {
    glfwSetWindowSize ( _window, width, height );
    _width = width;
    _height = height;
}

int GFXLayer::width () {
    return _width;
}

int GFXLayer::height () {
    return _height;
}

void GFXLayer::process_events () {
    glfwPollEvents();
    _input_handler ( ImGui::GetIO() );
}

bool GFXLayer::should_quit () {
    return glfwWindowShouldClose ( _window );
}

void GFXLayer::swap_buffers () {
    glfwSwapBuffers ( _window );
}

void* GFXLayer::get_window () {
    return _window;
}

void GFXLayer::update_config() {
    int width = Config::read_i ( Config::RENDER_WIDTH );
    int height = Config::read_i ( Config::RENDER_HEIGHT );

    if ( width != _width || height != _height ) {
        _resize ( width, height );
    }
}

GLFWwindow* GFXLayer::window_handle() {
    return _window;
}

const char* ShaderUniform::type_to_string(const GLenum type) {
    switch (type) {
        case GL_FLOAT: return "float";
        case GL_FLOAT_VEC2: return "vec2";
        case GL_FLOAT_VEC3: return "vec3";
        case GL_FLOAT_VEC4: return "vec4";
        case GL_FLOAT_MAT2: return "mat2";
        case GL_FLOAT_MAT3: return "mat3";
        case GL_FLOAT_MAT4: return "mat4";
        default: assert(false);
    }
}

void Pipeline::reset(
    GLuint rt_color,
    const char* shader_vert_src,
    const char* shader_frag_src,
    const int   width,
    const int   height
) {
    assert(shader_vert_src);
    assert(shader_frag_src);
    assert(width);
    assert(height);
    assert(glIsTexture(rt_color));

    glCreateTextures(GL_TEXTURE_2D, 1, &rt_depth); GL_NO_ERROR;
    glTextureParameteri(rt_depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); GL_NO_ERROR;
    glTextureParameteri(rt_depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER); GL_NO_ERROR;
    glTextureParameteri(rt_depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST); GL_NO_ERROR;
    glTextureParameteri(rt_depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST); GL_NO_ERROR;
    glTextureStorage2D(rt_depth, 1, GL_DEPTH_COMPONENT32, (GLsizei)width, (GLsizei)height); GL_NO_ERROR;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, rt_color, 0); GL_NO_ERROR;
    glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, rt_depth, 0); GL_NO_ERROR;

    shader_vert = _load_shader(GL_VERTEX_SHADER, shader_vert_src);
    shader_frag = _load_shader(GL_FRAGMENT_SHADER, shader_frag_src);
    assert(glIsShader(shader_vert));
    assert(glIsShader(shader_frag));

    program = glCreateProgram(); GL_NO_ERROR;
    glAttachShader(program, shader_vert); GL_NO_ERROR;
    glAttachShader(program, shader_frag); GL_NO_ERROR;
    glLinkProgram(program); GL_NO_ERROR;
    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status); GL_NO_ERROR;
    if (!link_status) {
        char linker_message[512];
        glGetProgramInfoLog(program, sizeof(linker_message), NULL, linker_message); GL_NO_ERROR;
        fprintf(stderr, "GLSL linker error %s\n", linker_message);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    _reflect_program();

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void Pipeline::bind() {
    assert(glIsProgram(program));
    assert(glIsFramebuffer(fbo));
    
    glUseProgram(program);
    //glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, 1280, 720);
}

const ShaderUniform& Pipeline::uniform(const char* str) {
    const auto uniform = _uniforms.find(str);
    assert(uniform != _uniforms.end());
    return uniform->second;
}

GLuint Pipeline::_load_shader(const GLenum stage, const char* glsl) {
    GLuint shader = glCreateShader(stage); GL_NO_ERROR;
    const GLint length = strlen(glsl);
    glShaderSource(shader, 1, &glsl, &length); GL_NO_ERROR;
    glCompileShader(shader); GL_NO_ERROR;
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar compile_message[512];
        glGetShaderInfoLog(shader, sizeof(compile_message), NULL, compile_message);
        fprintf(stderr, "GLSL compiler error %s\n", compile_message);
    }

    return shader;
}

void Pipeline::_reflect_program() {
    _uniforms.clear();
    
    assert(glIsProgram(program));
    GLint n_uniforms;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &n_uniforms);

    if (n_uniforms == 0)
        return;

    GLint uniform_max_length;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_max_length);
    auto uniform_name = std::make_unique<char[]>(uniform_max_length);

    for (GLint u = 0; u < n_uniforms; ++u) {
        ShaderUniform uniform;
        GLsizei length;
        glGetActiveUniform(program, u, uniform_max_length, &length, &uniform.length, &uniform.type, uniform_name.get());
        uniform.name = uniform_name.get();
        assert(_uniforms.find(uniform.name) == _uniforms.end());
        uniform.binding = glGetUniformLocation(program, uniform.name.c_str()); GL_NO_ERROR;

        fprintf(stderr, "found uniform %s length %d type %s binding %d \n", uniform.name.c_str(), uniform.length, ShaderUniform::type_to_string(uniform.type), uniform.binding);
        _uniforms[uniform.name] = uniform;
    }
}

#else
#error Implement graphics backend for your platform
#endif