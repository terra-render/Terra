// Satellite
#include <CameraControls.hpp>
#include <LinearAlgebra.hpp>
#include <Camera.hpp>

// glfw
#include <GLFW/glfw3.h>

// libc
#include <cstdio> // debug
#include <cstring>

void FirstPersonControls::update(
    GLFWwindow* wnd,
    Camera& camera, 
    const double dt
) {
    vec4 dir;
    memcpy(dir, camera.direction(), sizeof(vec3));
    dir[3] = 0.f;

    vec4 up;
    vec3_set(up, 0.f, 1.f, 0.f);
    up[3] = 0.;

    vec4 right, right_prev;
    vec3_mul_cross(right, dir, up);
    

    if (mouse_dragging) {
        double next_mouse_pos[2];
        glfwGetCursorPos(wnd, next_mouse_pos, next_mouse_pos + 1);
        const double mouse_dx = next_mouse_pos[0] - mouse_pos[0];
        const double mouse_dy = next_mouse_pos[1] - mouse_pos[1];
        printf("mouse delta %f %f\n", mouse_dx, mouse_dy);

        const double yaw_angle = mouse_dx * _speed_pixels_per_rad;
        const double pitch_angle = mouse_dy * _speed_pixels_per_rad;

        mat4 T_identity, T_yaw, T_pitch;
        mat4_identity(T_identity);
        mat4_rotate_Y(T_yaw, T_identity, yaw_angle);

        vec4 dir_next;
        mat4_mul_vec4(dir_next, T_yaw, dir);

        mat4_rotate(T_pitch, T_identity, -right[0], -right[1], -right[2], pitch_angle);
        mat4_mul_vec4(dir, T_pitch, dir_next);

        mouse_pos[0] = next_mouse_pos[0];
        mouse_pos[1] = next_mouse_pos[1];
    }

    // Construct new transform matrix
    vec3 forward, backward, left, down;
    vec3_set(forward, dir[0], dir[1], dir[2]);
    vec3_set(backward, -dir[0], -dir[1], -dir[2]);
    vec3_set(up, 0.f, 1.f, 0.f);
    vec3_mul_cross(right, forward, up);
    vec3_set(left, -right[0], -right[1], -right[2]);
    vec3_set(down, -up[0], -up[1], -up[2]);

    if (!mouse_dragging && glfwGetMouseButton(wnd, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        glfwGetCursorPos(wnd, mouse_pos, mouse_pos + 1);
        mouse_dragging = true;
    } else if (mouse_dragging && glfwGetMouseButton(wnd, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        mouse_dragging = false;
    }

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
    if (glfwGetKey(wnd, GLFW_KEY_W) == GLFW_PRESS) {
        vec3_add(delta, delta, forward);
    }
    else if (glfwGetKey(wnd, GLFW_KEY_S) == GLFW_PRESS) {
        vec3_add(delta, delta, backward);
    }
    else if (glfwGetKey(wnd, GLFW_KEY_A) == GLFW_PRESS) {
        vec3_add(delta, delta, left);
    }
    else if (glfwGetKey(wnd, GLFW_KEY_D) == GLFW_PRESS) {
        vec3_add(delta, delta, right);
    }
    else if (glfwGetKey(wnd, GLFW_KEY_Z) == GLFW_PRESS) {
        vec3_add(delta, delta, up);
    }
    else if (glfwGetKey(wnd, GLFW_KEY_X) == GLFW_PRESS) {
        vec3_add(delta, delta, down);
    }

    vec3 pos;
    vec3_scale(delta, delta, (dt * _speed_units_per_sec));
    vec3_add(pos, camera.position(), delta);
    camera.set_lookat(pos, dir);
}