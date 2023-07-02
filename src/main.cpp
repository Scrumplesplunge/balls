#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <string_view>
#include <vector>

constexpr float kRadius = 1.0f;  // Currently hard-coded in the shader.
constexpr float kScale = 25.0f;
constexpr float kBoundary = 5000 / kScale;
constexpr float kDeltaTime = 1.0/240;
constexpr glm::vec2 kGravity = glm::vec2(0, 50);
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
  glm::vec2 velocity;
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
    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_,
                             [](GLFWwindow* window, double x, double y) {
                               ((Game*)glfwGetWindowUserPointer(window))
                                   ->HandleMouseMove(glm::vec2(x, y));
                             });
    glfwSetMouseButtonCallback(
        window_, [](GLFWwindow* window, int button, int action, int mods) {
          ((Game*)glfwGetWindowUserPointer(window))
              ->HandleMouseButton(button, action);
        });

    GLuint buffers[3];
    glGenBuffers(3, buffers);
    auto [box_vertices, mvp, instances] = buffers;

    box_vertices_ = box_vertices;
    mvp_ = mvp;
    instances_ = instances;

    glBindBuffer(GL_ARRAY_BUFFER, box_vertices_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kBox), kBox, GL_STATIC_DRAW);
  }

  ~Game() {
    glfwSetCursorPosCallback(window_, nullptr);
    glfwSetMouseButtonCallback(window_, nullptr);
    glfwSetWindowUserPointer(window_, nullptr);
  }

  void Run() {
    double time = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      UpdateMatrices();

      const double now = glfwGetTime();
      const int required_ticks = (now - time) / kDeltaTime;
      const int bounded_ticks = std::min(6, required_ticks);
      if (const int missed = required_ticks - bounded_ticks; missed) {
        std::cerr << "Lag: missed " << missed
                  << (missed == 1 ? " tick.\n" : " ticks\n.");
      }
      for (int i = 0; i < bounded_ticks; i++) Update();
      time += kDeltaTime * required_ticks;

      Draw();
      glfwSwapBuffers(window_);
    }
  }

 private:
  void UpdateMatrices() {
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);

    glm::mat4 to_screen =
        glm::translate(glm::vec3(0.5 * width, 0.5 * height, 0)) *
        glm::scale(glm::vec3(kScale, kScale, 1.0f));
    from_screen_ = glm::inverse(to_screen);
    view_ = glm::ortho(0.0f, float(width), float(height), 0.0f, 1.0f, -1.0f) *
            to_screen;
  }

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
    glClear(GL_COLOR_BUFFER_BIT);

    struct {
      glm::mat4 matrix;
    } mvp;
    mvp.matrix = view_;

    glBufferData(GL_UNIFORM_BUFFER, sizeof(mvp), &mvp, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, mvp_);
    glBindBufferBase(GL_UNIFORM_BUFFER, kMvp, mvp_);

    DrawLines();
    DrawBalls();
  }

  void Update() {
    // Update the balls according to gravity.
    for (Ball& ball : balls_) {
      ball.velocity += kGravity * kDeltaTime;
      ball.position += ball.velocity * kDeltaTime;
    }

    // Remove balls which have moved far away from the origin.
    std::erase_if(balls_, [](const Ball& ball) {
      return glm::dot(ball.position, ball.position) > kBoundary * kBoundary;
    });

    // Randomly shuffle all balls and lines to prevent the order from mattering.
    std::shuffle(balls_.begin(), balls_.end(), gen_);
    std::shuffle(lines_.begin(), lines_.end(), gen_);

    constexpr double kCellSize = 2 * kRadius;
    constexpr int kGridRadius = 1 + std::ceil(kBoundary / kCellSize);
    constexpr int kGridSize = 4 * kGridRadius * kGridRadius;

    constexpr int kMaxCellSize = 15;
    struct Cell {
      void push_back(std::uint16_t x) {
        if (size == kMaxCellSize) throw std::runtime_error("Cell overflowed.");
        data[size++] = x;
      }

      std::uint16_t* begin() { return data; }
      std::uint16_t* end() { return data + size; }

      std::uint16_t size = 0;
      std::uint16_t data[kMaxCellSize];
    };

    std::vector<Cell> balls(kGridSize);

    struct Point { int x, y, i; };
    static constexpr auto cell = [](glm::vec2 position) {
      const int x = int(position.x / kCellSize) + kGridRadius;
      const int y = int(position.y / kCellSize) + kGridRadius;
      const int i = y * 2 * kGridRadius + x;
      return Point{.x = x, .y = y, .i = i};
    };

    for (int i = 0, n = balls_.size(); i < n; i++) {
      const Ball& b = balls_[i];
      const Point p = cell(b.position);
      balls[p.i].push_back(i);
    }

    // Check for collisions between lines and balls.
    for (const Line& line : lines_) {
      const glm::vec2 d = line.b - line.a;
      const Point a = cell(line.a);
      const Point b = cell(line.b);
      const int x_min = std::min(a.x, b.x) - 1;
      const int x_max = std::max(a.x, b.x) + 1;
      const int y_min = std::min(a.y, b.y) - 1;
      const int y_max = std::max(a.y, b.y) + 1;
      for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
          for (int ball_index : balls[2 * kGridRadius * y + x]) {
            Ball& ball = balls_[ball_index];

            // Check for a collision.
            const glm::vec2 v = ball.position - line.a;
            const float t =
                std::clamp(glm::dot(d, v) / glm::dot(d, d), 0.0f, 1.0f);
            const glm::vec2 p = line.a + t * d;
            const glm::vec2 offset = ball.position - p;
            const float square_distance = glm::dot(offset, offset);
            if (square_distance > kRadius * kRadius) continue;

            // Handle the collision.
            const float overlap = kRadius - std::sqrt(square_distance);
            const glm::vec2 normal = glm::normalize(offset);
            ball.position += 0.8f * overlap * normal;
            const float separation_speed = glm::dot(ball.velocity, normal);
            if (separation_speed < 0) {
              ball.velocity -= 1.8f * separation_speed * normal;
            }
          }
        }
      }
    }

    // Check for collisions between balls.
    int n = balls_.size();
    for (int i = 0; i < n; i++) {
      Ball& a = balls_[i];
      const Point p = cell(a.position);
      const int x_min = p.x - 1;
      const int x_max = p.x + 1;
      const int y_min = p.y - 1;
      const int y_max = p.y + 1;
      for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
          for (int ball_index : balls[2 * kGridRadius * y + x]) {
            if (ball_index == i) continue;

            // Check for a collision.
            Ball& b = balls_[ball_index];
            const glm::vec2 offset = b.position - a.position;
            const float square_distance = glm::dot(offset, offset);
            if (square_distance > 4 * kRadius * kRadius) continue;

            // Handle the collision.
            const float overlap = 2 * kRadius - std::sqrt(square_distance);
            const glm::vec2 normal = glm::normalize(offset);
            a.position -= 0.4f * overlap * normal;
            b.position += 0.4f * overlap * normal;
            const float separation_speed =
                glm::dot(b.velocity - a.velocity, normal);
            if (separation_speed < 0) {
              const glm::vec2 correction = 0.9f * separation_speed * normal;
              a.velocity += correction;
              b.velocity -= correction;
            }
          }
        }
      }
    }
  }

  void HandleMouseMove(glm::vec2 position) {
    mouse_ = glm::vec2(from_screen_ * glm::vec4(position, 0.0f, 1.0f));
    if (drawing_ && glm::distance(line_start_, mouse_) > 0.1) {
      lines_.push_back(Line{.a = line_start_, .b = mouse_});
      line_start_ = mouse_;
    }
  }

  void HandleMouseButton(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      if (action == GLFW_PRESS) {
        line_start_ = mouse_;
        drawing_ = true;
      } else if (action == GLFW_RELEASE) {
        if (glm::distance(line_start_, mouse_) > 0.01) {
          lines_.push_back(Line{.a = line_start_, .b = mouse_});
        }
        drawing_ = false;
      }
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
      balls_.push_back(Ball{.position = mouse_});
    }
  }

  std::ranlux24 gen_{std::random_device()()};
  GLFWwindow* const window_;
  const GLuint ball_shader_;
  const GLuint line_shader_;
  GLuint box_vertices_;
  GLuint line_vertices_;
  GLuint mvp_, instances_;
  std::vector<Ball> balls_;
  std::vector<Line> lines_;
  glm::mat4 view_, from_screen_;

  // Drawing state.
  bool drawing_ = false;
  glm::vec2 line_start_;
  glm::vec2 mouse_ = glm::vec2();
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
