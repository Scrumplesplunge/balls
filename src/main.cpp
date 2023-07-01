#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

int main() {
  glfwInit();
  GLFWwindow* window = glfwCreateWindow(640, 480, "Game", nullptr, nullptr);

  while (!glfwWindowShouldClose(window)) {
    glfwWaitEvents();
  }
}
