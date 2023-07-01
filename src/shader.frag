#version 430 core

layout(location = 0) in vec2 r;
layout(location = 0) out vec4 color;

void main() {
  const float x = dot(r, r) < 1.0f ? 1.0f : 0.0f;
  color = vec4(x, x, x, x);
}
