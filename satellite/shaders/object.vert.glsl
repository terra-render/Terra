#version 450 core

in float in_x;
in float in_y;
in float in_z;
in float in_nx;
in float in_ny;
in float in_nz;

out gl_PerVertex {
	vec4 gl_Position;
};

uniform mat4 u_clip_from_world;
uniform mat4 u_world_from_object;
uniform mat3 u_world_from_object_invT;

layout(location = 0) out vec3 normal_view;

void main() {
	const vec4 pos_object = vec4(in_x, in_y, in_z, 1.0);
	const vec3 normal_object = vec3(in_nx, in_ny, in_nz);

	const vec3 normal_world = u_world_from_object_invT * normal_object;
	gl_Position = u_clip_from_world * u_world_from_object * pos_object;
	normal_view = normal_world;
}