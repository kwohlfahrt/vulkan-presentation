#version 450
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec2 screen_pos;

void main() {
	gl_Position = vec4(screen_pos, 0.0, 1.0);
	gl_PointSize = 5.0;
}
