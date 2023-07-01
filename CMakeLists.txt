cmake_minimum_required(VERSION 3.26)

project(minimal-gl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)

add_library(glad src/third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC src/third_party/glad/include)

add_executable(game src/main.cpp)
target_link_libraries(game glad glfw)
