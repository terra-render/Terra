#pragma once

enum CameraControlFlags {
    CAMERA_CONTROL_NONE = 0,
    CAMERA_CONTROL_FORWARD = 1 << 0,
    CAMERA_CONTROL_BACKWARD = 1 << 1,
    CAMERA_CONTROL_LEFT = 1 << 2,
    CAMERA_CONTROL_RIGHT = 1 << 3,
    CAMERA_CONTROL_UP = 1 << 4,
    CAMERA_CONTROL_DOWN = 1 << 5
};
using CameraControlBits = int;

class Camera;

class CameraControls {
public:
    virtual ~CameraControls() = default;
    virtual void key_callback(int key, int scancode, int action, int mods) = 0;
    virtual void update(Camera& camera, const double dt) = 0;
};

class FirstPersonControls : public CameraControls {
public:
    void key_callback(int key, int scancode, int action, int mods) override;
    void update(Camera& camera, const double dt) override;

private:
    const double _speed = 1.;
    CameraControlBits _control_bits = CAMERA_CONTROL_NONE;
};