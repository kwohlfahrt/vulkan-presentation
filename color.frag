#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec3 color;

layout (location = 0) out vec4 out_color;

void main() {
     out_color = vec4(color, 1.0);
}
