#version 450

layout (triangles) in;
layout (location = 0) in vec3 pos[];
layout (location = 1) in vec3 color[];

layout (triangle_strip, max_vertices = 3) out;
layout (location = 0) out vec3 out_color;
layout (location = 1) out vec3 out_normal;

void main() {
  vec3 sides[2] = {pos[1] - pos[0], pos[2] - pos[0]};
  out_normal = normalize(cross(sides[0], sides[1]));
  
  for(uint i = 0; i < gl_in.length(); i++){
    out_color = color[i];
    gl_Position = gl_in[i % gl_in.length()].gl_Position;
    EmitVertex();
  }; 
  EndPrimitive();
}