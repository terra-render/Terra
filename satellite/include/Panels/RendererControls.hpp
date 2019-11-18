#pragma once

// satellite
#include <UI.hpp>

// libc++
#include <vector>
#include <string>

class RendererControls : public Panel {
public:
    ~RendererControls();
    RendererControls(const PanelSharedPtr& parent);

    void add_renderer(const char* name);
    void add_camera(const char* name);
    void add_camera_control(const char* name);

    void init() override;
    void draw() override;

private:
    void _on_select_renderer();
    void _on_select_camera();
    void _on_select_camera_controls();
    
    int _active_renderer = 0;
    int _active_camera = 0;
    int _active_camera_controls = 0;

    std::vector<char*> _renderer_names;
    std::vector<char*> _camera_names;
    std::vector<char*> _camera_control_names;
};