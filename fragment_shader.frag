#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

#define M_PI 3.1415926535897932384626433832795

layout (location = 0) in vec2 pos;
layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform Data {
       float time;
};

// https://stackoverflow.com/questions/15095909
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
     float h = mod(length(pos) / 2.0 + time, 1.0);
     vec3 hsv = vec3(h, 0.5, 0.8);
     out_color = vec4(hsv2rgb(hsv), 1.0);
}
