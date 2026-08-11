cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(PulsarSimulation LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure GLFW build options to avoid building docs, tests, and examples
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW library tests" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW library examples" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install target" FORCE)

# Configure GLAD build options before loading
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile to generate" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "API version to generate" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "Language binding generator" FORCE)
set(GLAD_EXPORT OFF CACHE BOOL "Export GLAD target" FORCE)
set(GLAD_INSTALL OFF CACHE BOOL "Generate GLAD install target" FORCE)

# Enable FetchContent
include(FetchContent)

# Declare GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

# Declare GLAD (v0.1.36 is standard and stable for OpenGL 3.x/4.x generation)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)

# Declare miniaudio (v0.11.21 is stable and single-header)
FetchContent_Declare(
    miniaudio
    URL      https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
)

# Populate GLFW, GLAD, and miniaudio
message(STATUS "Fetching GLFW...")
FetchContent_MakeAvailable(glfw)

message(STATUS "Fetching GLAD...")
FetchContent_MakeAvailable(glad)

message(STATUS "Fetching miniaudio...")
FetchContent_MakeAvailable(miniaudio)

# Create executable
add_executable(pulsar_sim main.cpp)

# Link dependencies
# GLAD handles OpenGL dynamic loading, and GLFW handles windowing and context.
# We also find and link the system OpenGL library.
find_package(OpenGL REQUIRED)

target_link_libraries(pulsar_sim PRIVATE glfw glad OpenGL::GL)
target_include_directories(pulsar_sim PRIVATE ${miniaudio_SOURCE_DIR})

# On macOS, GLFW automatically propagates its dependency on Cocoa, IOKit, and CoreVideo, so no manual framework links are needed.
