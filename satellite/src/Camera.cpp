// satellite
#include <Camera.hpp>

// libc++
#include <cstdio>

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
    //mat4_ortho(_clip_from_view, 0.f, (float)_width, 0.f, (float)_height, 0.f, 100.f);
    const float fov_y = M_PI_4;
    mat4_perspective(_clip_from_view, fov_y, (float)_width / _height, 1e-4, 1000.f);

#if 1
    printf("view_from_world\n");
    printf("%f %f %f %f\n", _view_from_world[0][0], _view_from_world[0][1], _view_from_world[0][2], _view_from_world[0][3]);
    printf("%f %f %f %f\n", _view_from_world[1][0], _view_from_world[1][1], _view_from_world[1][2], _view_from_world[1][3]);
    printf("%f %f %f %f\n", _view_from_world[2][0], _view_from_world[2][1], _view_from_world[2][2], _view_from_world[2][3]);
    printf("%f %f %f %f\n", _view_from_world[3][0], _view_from_world[3][1], _view_from_world[3][2], _view_from_world[3][3]);

    printf("clip_from_view\n");
    printf("%f %f %f %f\n", _clip_from_view[0][0], _clip_from_view[0][1], _clip_from_view[0][2], _clip_from_view[0][3]);
    printf("%f %f %f %f\n", _clip_from_view[1][0], _clip_from_view[1][1], _clip_from_view[1][2], _clip_from_view[1][3]);
    printf("%f %f %f %f\n", _clip_from_view[2][0], _clip_from_view[2][1], _clip_from_view[2][2], _clip_from_view[2][3]);
    printf("%f %f %f %f\n", _clip_from_view[3][0], _clip_from_view[3][1], _clip_from_view[3][2], _clip_from_view[3][3]);
#endif
}

void OrthographicCamera::set_lookat(const float x, const float y, const float z) {
    vec3 center;
    vec3_set(center, x, y, z);
    vec3_sub(_dir, center, _position);
    vec3_norm(_dir, _dir);
}