cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(PulsarSimulation LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW library tests" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW library examples" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install target" FORCE)

set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile to generate" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "API version to generate" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "Language binding generator" FORCE)
set(GLAD_EXPORT OFF CACHE BOOL "Export GLAD target" FORCE)
set(GLAD_INSTALL OFF CACHE BOOL "Generate GLAD install target" FORCE)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)

FetchContent_Declare(
    miniaudio
    URL      https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
)

FetchContent_Declare(
    imgui
    URL      https://github.com/ocornut/imgui/archive/refs/tags/v1.89.9.tar.gz
)

message(STATUS "Fetching GLFW...")
FetchContent_MakeAvailable(glfw)

message(STATUS "Fetching GLAD...")
FetchContent_MakeAvailable(glad)

message(STATUS "Fetching miniaudio...")
FetchContent_MakeAvailable(miniaudio)

message(STATUS "Fetching ImGui...")
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw glad)

add_executable(pulsar_sim main.cpp)

find_package(OpenGL REQUIRED)

target_link_libraries(pulsar_sim PRIVATE glfw glad OpenGL::GL imgui)
target_include_directories(pulsar_sim PRIVATE ${miniaudio_SOURCE_DIR})

