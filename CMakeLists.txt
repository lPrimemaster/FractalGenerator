cmake_minimum_required(VERSION 3.16)

project(CShader)

set(CMAKE_CXX_STANDARD 17)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw3)

find_package(OpenGL REQUIRED)

file(GLOB IMGUI_SRC
       ${PROJECT_SOURCE_DIR}/lib/imgui/*.cpp
       ${PROJECT_SOURCE_DIR}/lib/imgui/*.h)

add_executable(CShader
    src/main.cpp
    lib/glad/src/glad.c
    lib/glad/include/glad/glad.h
    lib/glad/include/KHR/khrplatform.h

    ${IMGUI_SRC}
    )

target_link_libraries(CShader glfw)
target_link_libraries(CShader OpenGL::GL)