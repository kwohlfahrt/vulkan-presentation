#version 450

layout (location = 0) in vec2 screen_pos;

layout (location = 0) out vec4 pos;
layout (location = 1) out vec4 color;

void main() {
  color = vec4((gl_VertexIndex + 0) % 3 == 0,
               (gl_VertexIndex + 1) % 3 == 0,
               (gl_VertexIndex + 2) % 3 == 0,
               1.0);
  gl_Position = vec4(screen_pos, 0.0, 1.0);
  gl_Position.x += float(gl_InstanceIndex) / 10.0;
  gl_Position.z += float(gl_InstanceIndex) / 5.0;
  pos = gl_Position;
}
