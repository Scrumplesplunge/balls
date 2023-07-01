cmake_minimum_required(VERSION 3.26)

project(minimal-gl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)

add_executable(game src/main.cpp)
target_link_libraries(game glfw)
