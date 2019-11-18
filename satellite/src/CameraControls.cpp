// Satellite
#include <CameraControls.hpp>
#include <LinearAlgebra.hpp>
#include <Camera.hpp>

// glfw
#include <GLFW/glfw3.h>

// libc
#include <cstdio> // debug

void FirstPersonControls::key_callback(int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_W) {
        _control_bits |= CAMERA_CONTROL_FORWARD;
    }
    else if (key == GLFW_KEY_A) {
        _control_bits |= CAMERA_CONTROL_LEFT;
    }
    else if (key == GLFW_KEY_D) {
        _control_bits |= CAMERA_CONTROL_RIGHT;
    }
    else if (key == GLFW_KEY_S) {
        _control_bits |= CAMERA_CONTROL_BACKWARD;
    }
    else if (key == GLFW_KEY_SPACE) {
        _control_bits |= CAMERA_CONTROL_UP;
    }
    else if (mods & GLFW_MOD_SHIFT) {
        _control_bits |= CAMERA_CONTROL_DOWN;
    }
}

void FirstPersonControls::update(Camera& camera, const double dt) {
#if 0
    // Construct new transform matrix


    vec3 forward, backward, left, right, up, down;
    vec3_set(forward, _dir[0], _dir[1], _dir[2]);
    vec3_set(backward, -_dir[0], -_dir[1], -_dir[2]);
    vec3_set(up, 0.f, 1.f, 0.f);
    vec3_mul_cross(right, forward, up);
    vec3_set(left, -right[0], -right[1], -right[2]);
    vec3_set(down, -up[0], -up[1], -up[2]);

#if 0
    printf("position %f %f %f\n", _position[0], _position[1], _position[2]);
    printf("direction %f %f %f\n", _dir[0], _dir[1], _dir[2]);
    printf("forward %f %f %f\n", forward[0], forward[1], forward[2]);
    printf("backward %f %f %f\n", backward[0], backward[1], backward[2]);
    printf("left %f %f %f\n", left[0], left[1], left[2]);
    printf("right %f %f %f\n", right[0], right[1], right[2]);
#endif

    vec3 delta;
    vec3_set1(delta, 0.f);
    if (_control_bits & CAMERA_CONTROL_FORWARD) {
        vec3_add(delta, delta, forward);
    }
    else if (_control_bits & CAMERA_CONTROL_BACKWARD) {
        vec3_add(delta, delta, backward);
    }
    else if (_control_bits & CAMERA_CONTROL_LEFT) {
        vec3_add(delta, delta, left);
    }
    else if (_control_bits & CAMERA_CONTROL_RIGHT) {
        vec3_add(delta, delta, right);
    }
    else if (_control_bits & CAMERA_CONTROL_UP) {
        vec3_add(delta, delta, up);
    }
    else if (_control_bits & CAMERA_CONTROL_DOWN) {
        vec3_add(delta, delta, down);
    }

    vec3_scale(delta, delta, (dt * _speed));
    vec3_add(_position, _position, delta);

#endif

    _control_bits = CAMERA_CONTROL_NONE;
}