#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

// gl_PrimitiveID seems to be broken
layout (location = 0) in vec3 in_color;
layout (location = 0) out vec4 out_color;

void main() {
  out_color = vec4(in_color, 1.0);
}
