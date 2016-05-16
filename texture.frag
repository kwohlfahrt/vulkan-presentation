#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler tex_sampler;
layout (set = 0, binding = 1) uniform texture2D tex;

void main() {
     out_color = texture(sampler2D(tex, tex_sampler), uv);
}
