#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <iostream>
#include <string_view>

[[noreturn]] void Die(std::string_view reason) {
  std::cerr << "Fatal error: " << reason << '\n';
  std::exit(1);
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

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    glfwSwapBuffers(window);
  }
}
