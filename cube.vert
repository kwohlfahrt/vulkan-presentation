#version 450
#extension KHR_vulkan_glsl : enable

#define M_PI 3.1415926535897932384626433832795

layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 out_pos;

mat4 perspective(float fov, float near, float far, float aspect) {
  fov = min(fov, M_PI);
  float s = 1 / tan(fov / 2);

  mat4 m = {
    {s / aspect, 0, 0                            , 0},
    {0,          s, 0                            , 0},
    {0,          0, (far + near) / (near - far)  ,-1},
    {0,          0, 2 * far * near / (near - far), 0}
  };

  return m;
}

mat4 translate(vec3 distance) {
  mat4 m = mat4(1.0);
  m[3] = vec4(distance, 1.0);
  return m;
}

mat4 lookAt(vec3 position, vec3 target, vec3 up) {
  vec3 f = normalize(target - position);
  vec3 s = normalize(cross(f, normalize(up)));
  vec3 u = cross(s, f);
  mat4 m = mat4(transpose(mat3(s, u, -f)));
  return m * translate(-position);
}

void main() {
  out_pos = pos * 0.5;
  vec3 instance_pos = vec3((gl_InstanceIndex) % 3,
                           (gl_InstanceIndex / 3) % 3,
                           (gl_InstanceIndex / 9) % 3) - 1.0;

  gl_Position = perspective(radians(90.0), 0.01, 10.0, 1920.0 / 1440.0)
              * lookAt(vec3( 1.5,-3.0, 4.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0))
              * vec4(out_pos * 0.8 + instance_pos, 1.0);
}
