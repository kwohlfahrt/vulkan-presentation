#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

// gl_PrimitiveID seems to be broken
layout (location = 0) in vec3 in_color;
layout (location = 1) in vec3 in_normal;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal;

void main() {
  out_color = vec4(in_color, 1.0);
  vec3 norm_normal = in_normal + 1.0 / 2; // Scale to +ive for display
  out_normal = vec4(norm_normal, 1.0);
}
