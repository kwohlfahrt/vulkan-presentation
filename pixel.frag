#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput in_color;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput in_normal;

layout (location = 0) in vec2 screen_pos;
layout (location = 0) out vec4 out_color;

void main() {
  vec3 color = subpassLoad(in_color).xyz;
  vec3 normal = subpassLoad(in_normal).xyz * 2.0 - 1.0;

  vec3 light_dir = normalize(vec3(-1.5,-2.0,-1.0));
  out_color = vec4(color * dot(-light_dir, normal), 1.0);
}