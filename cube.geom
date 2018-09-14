#version 450

layout (triangles) in;
layout (location = 0) in vec3 pos[];

layout (triangle_strip, max_vertices = 3) out;
layout (location = 0) out vec3 out_color;

void main() {
  vec3 sides[2] = {pos[1] - pos[0], pos[2] - pos[0]};
  vec3 normal = normalize(cross(sides[0], sides[1]));

  for(uint i = 0; i < gl_in.length(); i++){
    out_color = abs(normal) * 0.7;
    gl_Position = gl_in[i % gl_in.length()].gl_Position;
    EmitVertex();
  }; 
  EndPrimitive();
}