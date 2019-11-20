#pragma once

class Camera;
struct GLFWwindow;

class CameraControls {
public:
    virtual ~CameraControls() = default;
    virtual void update(
        GLFWwindow* wnd,
        Camera& camera,
        const double dt
    ) = 0;
};

class FirstPersonControls : public CameraControls {
public:
    void update (
        GLFWwindow* wnd,
        Camera& camera, 
        const double dt
    ) override;

private:
    double mouse_pos[2];
    bool mouse_dragging = false;
    const double _speed_units_per_sec = 10.;
    const double _speed_pixels_per_rad = -.001;
};