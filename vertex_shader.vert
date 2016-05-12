#version 450
#extension KHR_vulkan_glsl : enable

#define M_PI 3.1415926535897932384626433832795

layout (location = 0) in vec2 screen_pos;

layout (set = 0, binding = 0) uniform Data {
       float time;
};

void main() {
	gl_Position = vec4(screen_pos, 0.0, 1.0);
  gl_Position.x += sin(time * 2 * M_PI);
	gl_PointSize = 5.0;
}
