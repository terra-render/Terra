#version 450 core

in int gl_VertexID;

out gl_PerVertex {
	vec4 gl_Position;
};

layout(location = 0) out vec2 out_uv;

void main() {
	gl_Position.x = -1.0 + float((gl_VertexID & 1) << 2);
	gl_Position.y = -1.0 + float((gl_VertexID & 2) << 1);
	gl_Position.z = 0.0;
	gl_Position.w = 1.0;

	out_uv.x = (gl_Position.x+1.0)*0.5;
	out_uv.y = (gl_Position.y+1.0)*0.5;
}