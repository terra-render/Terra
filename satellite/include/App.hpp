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

    int  run();

  private:
    void _update();
    void _draw();

    int  _on_command ( const CommandArgs& args );
    void _on_resize ( int w, int h );

    void _init_ui();
    void _register_commands();
    void _boot();


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

    CommandCallback _c_help;
    CommandCallback _c_clear;
    CommandCallback _c_load;
    CommandCallback _c_step;
    CommandCallback _c_loop;
    CommandCallback _c_tonemap;
    CommandCallback _c_pause;
    CommandCallback _c_save;
    CommandCallback _c_option;
    CommandCallback _c_resize;
    CommandCallback _c_debug;
};