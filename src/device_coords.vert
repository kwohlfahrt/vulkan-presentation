#version 450

layout (location = 0) in vec2 screen_pos;

layout (location = 0) out vec3 color;

void main() {
	color = vec3(1.0);
	gl_Position = vec4(screen_pos, 0.0, 1.0);
	gl_PointSize = 5.0;
}
