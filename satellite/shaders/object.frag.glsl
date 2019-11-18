#version 450 core

layout(location = 0) in vec3 normal_view;

layout(location = 0) out vec4 color0;

void main() {
	color0 = vec4(1.0, 1.0, 1.0, 1.0);
}