#version 450 core

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color0;

uniform sampler2D u_texture;

void main() {
	out_color0 = vec4(texture(u_texture, in_uv).xyz, 1.0);
}