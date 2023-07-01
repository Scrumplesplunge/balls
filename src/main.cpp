#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

constexpr double kDeltaTime = 1.0/240;
constexpr int kVertex = 0;  // layout(location = 0) in vec2 vertex;
constexpr int kCenter = 1;  // layout(location = 1) in vec2 center;
constexpr int kMvp = 0;     // layout(binding = 0) uniform MVP { ... }

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

GLuint LoadShaderProgram(const std::filesystem::path& vertex_shader_path,
                         const std::filesystem::path& fragment_shader_path) {
  const GLuint vertex_shader =
      LoadShader(GL_VERTEX_SHADER, vertex_shader_path);
  const GLuint fragment_shader =
      LoadShader(GL_FRAGMENT_SHADER, fragment_shader_path);
  const GLuint program =
      LinkProgram(std::array{vertex_shader, fragment_shader});
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  return program;
}

struct Ball {
  glm::vec2 position;
};

struct Line {
  glm::vec2 a, b;
};

static constexpr float kBox[] = {
  -1.0f, -1.0f,
  -1.0f, 1.0f,
  1.0f, 1.0f,
  1.0f, -1.0f,
};
constexpr int kNumBoxVertices = sizeof(kBox) / (2 * sizeof(float));

class Game {
 public:
  Game(GLFWwindow* window)
      : window_(window),
        ball_shader_(LoadShaderProgram("src/ball.vert", "src/ball.frag")),
        line_shader_(LoadShaderProgram("src/line.vert", "src/line.frag")) {

    GLuint buffers[3];
    glGenBuffers(3, buffers);
    auto [box_vertices, mvp, instances] = buffers;

    box_vertices_ = box_vertices;
    mvp_ = mvp;
    instances_ = instances;

    glBindBuffer(GL_ARRAY_BUFFER, box_vertices_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kBox), kBox, GL_STATIC_DRAW);

    balls_ = {{.position = glm::vec2(0.0f, 0.0f)},
              {.position = glm::vec2(-1.5f, 0.0f)},
              {.position = glm::vec2(1.5f, 0.0f)}};
    lines_ = {{.a = glm::vec2(-2.6f, -1.1f), .b = glm::vec2(2.6f, -1.1f)},
              {.a = glm::vec2(-2.6f, 1.1f), .b = glm::vec2(2.6f, 1.1f)}};
  }

  void Run() {
    double time = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      const double now = glfwGetTime();
      while (time < now) {
        time += kDeltaTime;
        Update();
      }
      Draw();
      glfwSwapBuffers(window_);
    }
  }

 private:
  void DrawBalls() const {
    // Select the box vertex buffer.
    glBindBuffer(GL_ARRAY_BUFFER, box_vertices_);
    glEnableVertexAttribArray(kVertex);
    glVertexAttribPointer(kVertex, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Load the instance data for all balls.
    std::vector<glm::vec2> instances;
    instances.reserve(balls_.size());
    for (const Ball& ball : balls_) instances.push_back(ball.position);
    glBindBuffer(GL_ARRAY_BUFFER, instances_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * instances.size(),
                 instances.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(kCenter);
    glVertexAttribPointer(kCenter, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glVertexAttribDivisor(kCenter, 1);

    // Draw all the balls.
    glUseProgram(ball_shader_);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, kNumBoxVertices,
                          instances.size());

    // Disable the vertex arrays again.
    glDisableVertexAttribArray(kVertex);
    glDisableVertexAttribArray(kCenter);
  }

  void DrawLines() const {
    // Load the vertices for all lines.
    std::vector<glm::vec2> vertices;
    vertices.reserve(2 * lines_.size());
    for (const Line& line : lines_) {
      vertices.push_back(line.a);
      vertices.push_back(line.b);
    }
    glBindBuffer(GL_ARRAY_BUFFER, instances_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vertices.size(),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(kVertex);
    glVertexAttribPointer(kVertex, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Draw all the lines.
    glUseProgram(line_shader_);
    glDrawArrays(GL_LINES, 0, vertices.size());

    // Disable the vertex array.
    glDisableVertexAttribArray(kVertex);
  }

  void Draw() const {
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    struct {
      glm::mat4 matrix;
    } mvp;
    mvp.matrix =
        glm::ortho(0.0f, float(width), float(height), 0.0f, 1.0f, -1.0f) *
        glm::translate(glm::vec3(0.5 * width, 0.5 * height, 0)) *
        glm::scale(glm::vec3(100.0f, 100.0f, 1.0f));

    glBufferData(GL_UNIFORM_BUFFER, sizeof(mvp), &mvp, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, mvp_);
    glBindBufferBase(GL_UNIFORM_BUFFER, kMvp, mvp_);

    DrawLines();
    DrawBalls();
  }

  void Update() {
    time_ += kDeltaTime;
    balls_[0].position.x = std::cos(time_);
    balls_[0].position.y = std::sin(time_);
  }

  GLFWwindow* const window_;
  const GLuint ball_shader_;
  const GLuint line_shader_;
  GLuint box_vertices_;
  GLuint line_vertices_;
  GLuint mvp_, instances_;
  double time_ = 0;
  std::vector<Ball> balls_;
  std::vector<Line> lines_;
};

int main() {
  if (!glfwInit()) Die("glfwInit");
  GLFWwindow* window = glfwCreateWindow(640, 480, "Game", nullptr, nullptr);
  if (!window) Die("glfwCreateWindow");
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  if (!gladLoadGL(glfwGetProcAddress)) Die("gladLoadGL");

  GLuint vertex_array;
  glGenVertexArrays(1, &vertex_array);
  glBindVertexArray(vertex_array);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

  Game game(window);
  game.Run();
}
