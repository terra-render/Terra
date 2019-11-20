// satellite
#include <Camera.hpp>

// libc++
#include <cstdio>
#include <cstring>

Camera::Camera() {
    mat4_identity(_view_from_world);
    mat4_identity(_clip_from_view);
}

void Camera::set_lookat(
    const float* position,
    const float* direction
) {
    vec3_set(_position, position[0], position[1], position[2]);
    vec3_set(_direction, direction[0], direction[1], direction[2]);
    vec3 center;
    vec3_add(center, _position, _direction);
    vec3 up;
    vec3_set(up, 0.f, 1.f, 0.f);

    mat4_look_at(_view_from_world, _position, center, up);
}

void Camera::resize(
    const unsigned int width,
    const unsigned int height
) {
    _width = width;
    _height = height;
}

void OrthographicCamera::resize(const unsigned int width, const unsigned int height) {
    Camera::resize(width, height);

    const float fov_y = M_PI_4;
    mat4_perspective(_clip_from_view, fov_y, (float)_width / _height, 1e-4, 1000.f);
}