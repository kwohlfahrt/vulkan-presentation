#version 450
#extension KHR_vulkan_glsl : enable

layout (location = 0) out vec2 screen_pos;

void main() {
	screen_pos = vec2(gl_VertexIndex / 2, gl_VertexIndex % 2) * 2 - 1;
	gl_Position = vec4(screen_pos, 0.0, 1.0);
}
