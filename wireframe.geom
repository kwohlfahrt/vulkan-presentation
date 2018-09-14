#version 450

layout (triangles) in;
layout (location = 0) in vec3 pos[];

layout (line_strip, max_vertices = 4) out;
layout (location = 0) out vec3 color;

void main() {
  color = vec3(1.0);
  for(uint i = 0; i < gl_in.length(); i++){
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
  }; 
  gl_Position = gl_in[0].gl_Position;
  EmitVertex();
  EndPrimitive();
}