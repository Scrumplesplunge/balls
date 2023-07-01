#version 430 core

layout(binding = 0) uniform MVP {
  mat4 matrix;
};

layout(location = 0) in vec2 vertex;

void main() {
  gl_Position = matrix * vec4(vertex, 0.0, 1.0);
}
