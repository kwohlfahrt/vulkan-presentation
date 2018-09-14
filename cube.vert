#version 450

#define M_PI 3.1415926535897932384626433832795

layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 out_pos;
layout (push_constant) uniform View {
       uint persp;
} view;

// https://msdn.microsoft.com/en-us/library/windows/desktop/bb147302(v=vs.85).aspx
mat4 perspective(float fov, float near, float far, float aspect) {
  fov = min(fov, M_PI);
  float s = 1 / tan(fov / 2);

  mat4 m = {
    {s / aspect, 0, 0                         , 0},
    {0,          s, 0                         , 0},
    {0,          0, far / (far - near)        , 1},
    {0,          0, -far * near / (far - near), 0}
  };

  return m;
}

mat4 ortho(float scale, float near, float far, float aspect) {
  mat4 m = {
    {scale / aspect, 0,     0                   , 0},
    {0,              scale, 0                   , 0},
    {0,              0,     1 / (far - near)    , 0},
    {0,              0,     -near / (far - near), 1}
  };

  return m;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/bb281710(v=vs.85).aspx
mat4 lookAt(vec3 position, vec3 target, vec3 up) {
  vec3 zaxis = normalize(target - position);
  vec3 xaxis = normalize(cross(normalize(up), zaxis));
  vec3 yaxis = cross(zaxis, xaxis);

  mat4 m = mat4(transpose(mat3(xaxis, yaxis, zaxis)));
  m[3] = vec4(-dot(xaxis, position), -dot(yaxis, position), -dot(zaxis, position), 1.0);
  return m;
}

void main() {
  out_pos = pos * 0.5;
  vec3 instance_pos = vec3((gl_InstanceIndex) % 3,
                           (gl_InstanceIndex / 3) % 3,
                           (gl_InstanceIndex / 9) % 3) - 1.0;
  mat4 proj = bool(view.persp) ?
    perspective(radians(80.0), 1.0, 10.0, 1920.0 / 1440.0) : 
    ortho(0.3, 1.0, 10.0, 1920.0 / 1440.0);

  gl_Position = proj
              * lookAt(vec3(-1.5,-3.0,-4.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0))
              * vec4(out_pos * 0.8 + instance_pos, 1.0);
}
