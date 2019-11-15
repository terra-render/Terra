#pragma once

#include <LinearAlgebra.hpp>

class Camera {
public:
    Camera();
    virtual ~Camera() = default;

    virtual void resize (
        const unsigned int width,
        const unsigned int height
    );
    virtual void update_transformations() = 0;

    void set_position (
        const float x,
        const float y,
        const float z
    );

    const float* position() const { return _position; }
    const mat4* view_from_world() const { return &_view_from_world; }
    const mat4* clip_from_view() const { return &_clip_from_view; }
    unsigned int width() const { return _width; }
    unsigned int height() const { return _height; }

protected:
    unsigned int _width, _height;
    mat4 _clip_from_view;
    mat4 _view_from_world;
    vec3 _position;
};

class OrthographicCamera : public Camera {
public:
    OrthographicCamera();
    
    void set_lookat(
        const float x,
        const float y,
        const float z
    );

    void update_transformations() override;

protected:
    vec3 _dir;
};