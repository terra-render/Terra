// Header
#include <Graphics.hpp>

// glew
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

// Satellite
#include <Logging.hpp>

// imgui
#include <examples/opengl3_example/imgui_impl_glfw_gl3.h>

#ifdef _WIN32

// Skipping notification, reporting everything else
void GLAPIENTRY opengl_debug_callback ( GLenum source, GLenum type, GLuint id, GLenum severity,
                                        GLsizei length, const char* message, const void* gfx_ptr ) {
    GFXLayer* gfx = ( GFXLayer* ) gfx_ptr;

    if ( severity == GL_DEBUG_SEVERITY_NOTIFICATION ) {
        return;
    }

    if ( severity == GL_DEBUG_SEVERITY_HIGH ) {
        Log::error ( "OpenGL Error: %s\n", message );
    } else {
        Log::warning ( "OpenGL Warning: %s\n", message );
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
    _on_resize      = on_resize;
    _input_handler  = input_handler;
    _width          = width;
    _height         = height;
    glfwSetWindowUserPointer ( _window, this );
    // Setting initial
    glViewport ( 0, 0, width, height );
    return true;
}

void GFXLayer::resize ( int width, int height ) {
    glfwSetWindowSize ( _window, width, height );
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

#else
#error Implement graphics backend for your platform
#endif