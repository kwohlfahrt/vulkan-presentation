#version 450

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 out_color;
layout (location = 1) out float out_depth;

void main() {
     out_color = color;
     out_color.rgb *= 1.0 - pos.z;
     out_depth = pos.z;
}
