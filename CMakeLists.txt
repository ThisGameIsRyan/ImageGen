cmake_minimum_required(VERSION 3.5.0)
project(hello_window VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(hello_window 
    src/config.h 
    src/main.cpp 
    src/glad.c
    src/triangle_mesh.h
    src/triangle_mesh.cpp
)

target_include_directories(hello_window PRIVATE dependencies)

target_link_libraries(hello_window ${GLFW3_LIBRARY} OpenGL::GL)