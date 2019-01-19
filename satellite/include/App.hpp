#pragma once

// Satellite
#include <Visualization.hpp>
#include <Events.hpp>
#include <Renderer.hpp>
#include <Graphics.hpp>
#include <Scene.hpp>
#include <Console.hpp>

// stdlib
#include <vector>
#include <memory>
#include <functional>

// The App is a container for Window, Presentation layers
// registers itself to the UI callbacks and passes events around
class App {
  public:
    App ( int argc, char** argv );
    ~App();

    int run ();

  private:
    void _init_ui();
    void _init_cmd_map();
    void _boot();
    void _clear ();

    void _opt_set ( int opt, const std::string& value );
    void _opt_set ( int opt, int value );
    void _opt_set ( bool clear_check, std::function<void() > setter );
    void _on_opt_set ( int opt );

    GFXLayer      _gfx;
    Scene         _scene;
    TerraRenderer _renderer;

    // Presentation
    Console     _console;
    Visualizer  _visualizer;
    TerraCamera _render_camera;

    // Callbacks
    using CommandMap = std::map<std::string, CommandCallback>;
    CommandMap      _c_map;
};