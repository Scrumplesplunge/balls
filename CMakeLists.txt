cmake_minimum_required(VERSION 3.26)

project(minimal-gl)

find_package(glfw3 REQUIRED)

add_executable(game src/main.cpp)
target_link_libraries(game glfw)
