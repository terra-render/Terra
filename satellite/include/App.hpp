#pragma once

// Satellite
#include <Graphics.hpp>
#include <Scene.hpp>
#include <Camera.hpp>
#include <UI.hpp>

// stdlib
#include <vector>
#include <memory>
#include <functional>
#include <map>

// forward declarations
class Renderer;
class Camera;
class CameraControls;

// The App is a container for Window, Presentation layers
// registers itself to the UI callbacks and passes events around
class App {
  public:
    App ( int argc, char** argv );
    ~App();

    int run ();

  private:
    int  _boot();
    void _shutdown();
    void _register_message_listener();

    // This should not go through the App
    int _opt_set ( int opt, int value );
    int _opt_set ( int opt, const std::string& value );
    int _opt_set ( bool clear, std::function<int() > setter );

    // update dynamic components
    void _set_ui       ();
    void _set_renderer ( const std::string& type );
    void _set_camera   ( const std::string& type );
    void _set_camera_controls(const std::string& type);

    void _clear();

    void _on_config_change ( bool clear );

    // callback
    friend void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // timesteps
    double _dt;
    double _time_prev;

    // fixed components
    GFXLayer _gfx;
    Scene    _scene;
    PanelSharedPtr _ui;

    // dynamic components
    std::unique_ptr<Renderer>       _renderer;
    std::unique_ptr<Camera>         _camera;
    std::unique_ptr<CameraControls> _camera_controls;
};