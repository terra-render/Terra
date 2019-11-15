// satellite
#include <Camera.hpp>

Camera::Camera() {
    mat4_identity(_view_from_world);
    mat4_identity(_clip_from_view);
    vec3_set1(_position, 0.f);
}

void Camera::resize(
    const unsigned int width,
    const unsigned int height
) {
    _width = width;
    _height = height;
}

void Camera::set_position(
    const float x,
    const float y,
    const float z
) {
    vec3_set(_position, x, y, z);
}

OrthographicCamera::OrthographicCamera() {
    vec3_set(_dir, 0.f, 0.f, 1.f);
}

void OrthographicCamera::update_transformations() {
    vec3 up;
    vec3_set(up, 0.f, 1.f, 0.f);

    mat4_look_at(_view_from_world, _position, _dir, up);
    mat4_ortho(_clip_from_view, 0.f, (float)_width, 0.f, (float)_height, 0.f, 100.f);
}

void OrthographicCamera::set_lookat(const float x, const float y, const float z) {
    vec3 center;
    vec3_set(center, x, y, z);
    vec3_sub(_dir, center, _position);
    vec3_norm(_dir, _dir);
}