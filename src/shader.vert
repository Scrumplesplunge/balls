#version 430 core

layout(binding = 0) uniform MVP {
  mat4 matrix;
};

layout(location = 0) in vec2 vertex;
layout(location = 1) in vec2 center;
layout(location = 0) out vec2 r;

void main() {
  r = vertex;
  gl_Position = matrix * vec4(vertex + center, 0.0, 1.0);
}
