#pragma once

#include <LinearAlgebra.hpp>

class Camera {
public:
    Camera();
    virtual ~Camera() = default;

    virtual void resize ( const unsigned int width, const unsigned int height );
    void set_lookat(
        const float* position,
        const float* direction
    );

    const mat4* view_from_world() const { return &_view_from_world; }
    const mat4* clip_from_view() const { return &_clip_from_view; }
    unsigned int width() const { return _width; }
    unsigned int height() const { return _height; }

protected:
    // Sensor plane pixel dimensions
    unsigned int _width, _height;
    vec3 _position;
    vec3 _direction;

    // Image plane transformations
    mat4 _clip_from_view;
    mat4 _view_from_world;
};

class OrthographicCamera : public Camera {
public:
    void resize(const unsigned int width, const unsigned int height) override;
};