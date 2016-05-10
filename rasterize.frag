#version 450
// glslang enables this but doesn't advertise it
#extension KHR_vulkan_glsl : enable

layout (location = 0) in vec2 screen_pos;

layout (location = 0) out vec4 out_color;

vec2 lines[3][2] = {
     {vec2( 0.0, 0.5), vec2(-0.5,-0.5)},
     {vec2(-0.5,-0.5), vec2( 0.5,-0.5)},
     {vec2( 0.5,-0.5), vec2( 0.0, 0.5)},
};

void main() {
     float d = - 1.0 / 0.0;
     for (uint i = 0; i < lines.length(); i++){
         vec2 line = lines[i][1] - lines[i][0];
         vec2 normal = normalize(vec2(line.y, - line.x));
         float new_d = dot(screen_pos - lines[i][0], normal);
         d = max(d, new_d);
     }
     d = d * 2;
     out_color = vec4(-min(d, 0.0), 0.0, max(d, 0.0), 1.0);
}
