#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>

[[noreturn]] void Die(std::string_view reason) {
  std::cerr << "Fatal error: " << reason << '\n';
  std::exit(1);
}

std::string GetContents(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::string contents{std::istreambuf_iterator<char>(file), {}};
  if (!file.good()) Die("GetContents");
  return contents;
}

GLuint LoadShader(GLuint type, const std::filesystem::path& path) {
  const std::string code = GetContents(path);
  GLuint shader = glCreateShader(type);
  const GLchar* const source = code.data();
  const GLint source_length = code.size();
  glShaderSource(shader, 1, &source, &source_length);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) Die("glCompileShader");
  return shader;
}

GLuint LinkProgram(std::span<const GLuint> shaders) {
  GLuint program = glCreateProgram();
  for (GLuint shader : shaders) glAttachShader(program, shader);
  glLinkProgram(program);

  GLint linked, validated;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) Die("glLinkProgram");
  glValidateProgram(program);
  glGetProgramiv(program, GL_VALIDATE_STATUS, &validated);
  if (!validated) Die("glValidateProgram");
  return program;
}

int main() {
  if (!glfwInit()) Die("glfwInit");
  GLFWwindow* window = glfwCreateWindow(640, 480, "Game", nullptr, nullptr);
  if (!window) Die("glfwCreateWindow");
  glfwMakeContextCurrent(window);
  if (!gladLoadGL(glfwGetProcAddress)) Die("gladLoadGL");

  GLuint vertex_array;
  glGenVertexArrays(1, &vertex_array);
  glBindVertexArray(vertex_array);

  const GLuint vertex_shader = LoadShader(GL_VERTEX_SHADER, "src/shader.vert");
  const GLuint fragment_shader =
      LoadShader(GL_FRAGMENT_SHADER, "src/shader.frag");
  const GLuint program =
      LinkProgram(std::array{vertex_shader, fragment_shader});
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glUseProgram(program);

  static constexpr float kTriangle[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
     0.0f,  1.0f,
  };

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kTriangle), kTriangle, GL_STATIC_DRAW);
  constexpr int kPosition = 0;  // layout(location = 0) in vec2 position;
  glEnableVertexAttribArray(kPosition);
  glVertexAttribPointer(kPosition, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
  }
}
