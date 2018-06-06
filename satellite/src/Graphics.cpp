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

struct _GFXLayer {
    GLFWwindow*      window;
    OnResizeCallback on_resize;
    InputHandler     input_handler;

    // Cached
    int width;
    int height;
};

// Skipping notification, reporting everything else
void GLAPIENTRY opengl_debug_callback ( GLenum source, GLenum type, GLuint id, GLenum severity,
                                        GLsizei length, const char* message, const void* user_ptr ) {
    _GFXLayer* gfx = ( _GFXLayer* ) user_ptr;

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

GFXLayer gfx_init ( int width, int height, const char* title, const OnResizeCallback& on_resize, const InputHandler& input_handler ) {
    _GFXLayer* gfx = new _GFXLayer();
    glfwSetErrorCallback ( glfw_error_callback );
    bool enable_opengl_debug = false;
#ifdef _DEBUG
    enable_opengl_debug = true;
#endif

    if ( !glfwInit() ) {
        Log::error ( STR ( "Failed to initialize GLFW" ) );
        delete gfx;
        return nullptr;
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
    gfx->window = glfwCreateWindow ( width, height, title, nullptr, nullptr );
    glfwSetWindowPos ( gfx->window, 100, 100 );
    glfwMakeContextCurrent ( gfx->window );
    glfwSwapInterval ( 1 ); // vsync
    gl3wInit(); // opengl function pointers

    // Opengl error messages
    if ( enable_opengl_debug ) {
        glEnable ( GL_DEBUG_OUTPUT );
    }

    glDebugMessageCallback ( opengl_debug_callback, gfx );
    // Initializing GFXLayer
    gfx->on_resize      = on_resize;
    gfx->input_handler  = input_handler;
    gfx->width          = width;
    gfx->height         = height;
    glfwSetWindowUserPointer ( gfx->window, gfx );
    // Setting initial
    glViewport ( 0, 0, width, height );
    return gfx;
}

void gfx_resize ( GFXLayer gfx, int width, int height ) {
    glfwSetWindowSize ( ( ( _GFXLayer* ) gfx )->window, width, height );
}

int gfx_width ( GFXLayer gfx ) {
    if ( gfx == nullptr ) {
        return -1;
    }

    return ( ( _GFXLayer* ) gfx )->width;
}

int gfx_height ( GFXLayer gfx ) {
    if ( gfx == nullptr ) {
        return -1;
    }

    return ( ( _GFXLayer* ) gfx )->height;
}

void gfx_process_events ( GFXLayer gfx ) {
    glfwPollEvents();
    ( ( _GFXLayer* ) gfx )->input_handler ( ImGui::GetIO() );
}

bool gfx_should_quit ( GFXLayer gfx ) {
    return glfwWindowShouldClose ( ( ( _GFXLayer* ) gfx )->window );
}

void gfx_swap_buffers ( GFXLayer gfx ) {
    glfwSwapBuffers ( ( ( _GFXLayer* ) gfx )->window );
}

void* gfx_get_window ( GFXLayer gfx ) {
    return ( ( _GFXLayer* ) gfx )->window;
}

#else
#error Implement graphics backend for your platform
#endif