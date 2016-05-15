#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 out_color;

void main() {
     out_color = color;
     out_color.rgb *= 1.0 - pos.z;
}
