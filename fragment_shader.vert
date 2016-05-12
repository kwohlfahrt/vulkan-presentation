#version 450
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec2 screen_pos;
layout (location = 0) out vec2 pos;

void main() {
	gl_Position = vec4(screen_pos, 0.0, 1.0);
  pos = screen_pos * 2;
}
