#version 450
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec2 screen_pos;
layout (location = 1) in vec2 in_uv;

layout (location = 0) out vec2 uv;

void main() {
	gl_Position = vec4(screen_pos, 0.0, 1.0);
  uv = in_uv;
}
