#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cmath>
#include <iomanip>
#include <atomic>
#include <cstdlib>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void checkCompileErrors(unsigned int shader, std::string type);
void processInput(GLFWwindow* window, float deltaTime);
void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

std::atomic<float> period{1.0f};
std::atomic<float> inclination_deg{30.0f};
std::atomic<bool> show_waves{true};
std::atomic<bool> show_magnetic_fields{true};
std::atomic<bool> show_info{false};

// Camera state and thread-safe atomic normals for audio synchronization
std::atomic<float> cameraYaw{0.0f};
std::atomic<float> cameraPitch{0.2f};
std::atomic<float> cameraDistance{5.5f};
std::atomic<bool> isMousePressed{false};
std::atomic<float> cameraNx{0.0f};
std::atomic<float> cameraNy{0.0f};
std::atomic<float> cameraNz{-1.0f};


const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

out vec2 TexCoords;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoords = aPos * 0.5 + 0.5;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec2 u_resolution;
uniform float u_time;
uniform float u_period;
uniform float u_inclination;
uniform int u_show_waves;
uniform int u_show_magnetic_fields;
uniform int u_show_info;
uniform vec3 u_camera_pos;

float hash(vec3 p) {
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p.xyz, p.yzx + 19.19);
    return fract(p.x * p.y * p.z);
}

float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(p + vec3(0.0, 0.0, 0.0));
    float b = hash(p + vec3(1.0, 0.0, 0.0));
    float c = hash(p + vec3(0.0, 1.0, 0.0));
    float d = hash(p + vec3(1.0, 1.0, 0.0));
    float e = hash(p + vec3(0.0, 0.0, 1.0));
    float g = hash(p + vec3(1.0, 0.0, 1.0));
    float h = hash(p + vec3(0.0, 1.0, 1.0));
    float i = hash(p + vec3(1.0, 1.0, 1.0));

    return mix(
        mix(mix(a, b, f.x), mix(c, d, f.x), f.y),
        mix(mix(e, g, f.x), mix(h, i, f.x), f.y),
        f.z
    );
}

float drawSegment(vec2 p, vec2 a, vec2 b, float r) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return smoothstep(r, 0.0, length(pa - ba * h));
}

float drawLetter(int letter, vec2 p, vec2 size) {
    vec2 uv = p / size;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    
    float d = 0.0;
    float th = 0.038;
    
    if (letter == 80) { // P
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.5), vec2(0.6, 0.9), th));
    } else if (letter == 73) { // I
        d = max(d, drawSegment(uv, vec2(0.4, 0.1), vec2(0.4, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 77) { // M
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.4, 0.4), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.9), vec2(0.4, 0.4), th));
    } else if (letter == 87) { // W
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.3, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.9), vec2(0.5, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.3, 0.1), vec2(0.4, 0.6), th));
        d = max(d, drawSegment(uv, vec2(0.5, 0.1), vec2(0.4, 0.6), th));
    } else if (letter == 65) { // A
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.4, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.4, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.3, 0.4), vec2(0.5, 0.4), th));
    } else if (letter == 84) { // T
        d = max(d, drawSegment(uv, vec2(0.4, 0.1), vec2(0.4, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
    } else if (letter == 69) { // E
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.5, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 82) { // R
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.5), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.3, 0.5), vec2(0.6, 0.1), th));
    } else if (letter == 79) { // O
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 68) { // D
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.5, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.5, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.5, 0.1), vec2(0.6, 0.3), th));
        d = max(d, drawSegment(uv, vec2(0.5, 0.9), vec2(0.6, 0.7), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.3), vec2(0.6, 0.7), th));
    } else if (letter == 78) { // N
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.1), th));
    } else if (letter == 83) { // S
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.5), th));
    } else if (letter == 86) { // V
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.4, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.9), vec2(0.4, 0.1), th));
    } else if (letter == 76) { // L
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 71) { // G
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.4, 0.5), vec2(0.6, 0.5), th));
    } else if (letter == 70) { // F
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.5, 0.5), th));
    } else if (letter == 67) { // C
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 72) { // H
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
    } else if (letter == 85) { // U
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 75) { // K
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.1), th));
    } else if (letter == 66) { // B
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.55, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.55, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.55, 0.5), vec2(0.55, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
    } else if (letter == 81) { // Q
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
        d = max(d, drawSegment(uv, vec2(0.4, 0.3), vec2(0.65, 0.05), th));
    } else if (letter == 48) { // 0
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 49) { // 1
        d = max(d, drawSegment(uv, vec2(0.4, 0.1), vec2(0.4, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 50) { // 2
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.5), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 51) { // 3
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 52) { // 4
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
    } else if (letter == 53) { // 5
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 54) { // 6
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 55) { // 7
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
    } else if (letter == 56) { // 8
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.1), th));
    } else if (letter == 57) { // 9
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.2, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.1), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.9), vec2(0.6, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
    } else if (letter == 61) { // =
        d = max(d, drawSegment(uv, vec2(0.2, 0.6), vec2(0.6, 0.6), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.4), vec2(0.6, 0.4), th));
    } else if (letter == 46) { // .
        d = max(d, drawSegment(uv, vec2(0.4, 0.1), vec2(0.4, 0.2), th * 1.5));
    } else if (letter == 42) { // *
        d = max(d, drawSegment(uv, vec2(0.4, 0.3), vec2(0.4, 0.7), th));
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.6, 0.5), th));
    } else if (letter == 47) { // /
        d = max(d, drawSegment(uv, vec2(0.2, 0.1), vec2(0.6, 0.9), th));
    } else if (letter == 94) { // ^
        d = max(d, drawSegment(uv, vec2(0.2, 0.5), vec2(0.4, 0.9), th));
        d = max(d, drawSegment(uv, vec2(0.6, 0.5), vec2(0.4, 0.9), th));
    }
    return d;
}

float drawString_PERIOD(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 6.0;
    d = max(d, drawLetter(80, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // P
    d = max(d, drawLetter(69, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // E
    d = max(d, drawLetter(82, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // R
    d = max(d, drawLetter(73, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // I
    d = max(d, drawLetter(79, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // O
    d = max(d, drawLetter(68, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // D
    return d;
}

float drawString_INCLIN(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 6.0;
    d = max(d, drawLetter(73, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // I
    d = max(d, drawLetter(78, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // N
    d = max(d, drawLetter(67, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // C
    d = max(d, drawLetter(76, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // L
    d = max(d, drawLetter(73, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // I
    d = max(d, drawLetter(78, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // N
    return d;
}

float drawString_MAG(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 3.0;
    d = max(d, drawLetter(77, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // M
    d = max(d, drawLetter(65, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(71, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // G
    return d;
}

float drawString_WAVES(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 5.0;
    d = max(d, drawLetter(87, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // W
    d = max(d, drawLetter(65, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(86, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // V
    d = max(d, drawLetter(69, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // E
    d = max(d, drawLetter(83, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // S
    return d;
}

float drawString_VELA(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 4.0;
    d = max(d, drawLetter(86, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // V
    d = max(d, drawLetter(69, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // E
    d = max(d, drawLetter(76, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // L
    d = max(d, drawLetter(65, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // A
    return d;
}

float drawString_PULSAR(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 6.0;
    d = max(d, drawLetter(80, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // P
    d = max(d, drawLetter(85, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // U
    d = max(d, drawLetter(76, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // L
    d = max(d, drawLetter(83, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // S
    d = max(d, drawLetter(65, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(82, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // R
    return d;
}

float drawString_DEV(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 3.0;
    d = max(d, drawLetter(68, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // D
    d = max(d, drawLetter(69, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // E
    d = max(d, drawLetter(86, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // V
    return d;
}

float drawString_MATH(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 4.0;
    d = max(d, drawLetter(77, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // M
    d = max(d, drawLetter(65, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(84, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // T
    d = max(d, drawLetter(72, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // H
    return d;
}

float drawString_DAKSH(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 5.0;
    d = max(d, drawLetter(68, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // D
    d = max(d, drawLetter(65, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(75, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // K
    d = max(d, drawLetter(83, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // S
    d = max(d, drawLetter(72, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // H
    return d;
}

float drawString_HUTANSH(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 7.0;
    d = max(d, drawLetter(72, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // H
    d = max(d, drawLetter(85, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // U
    d = max(d, drawLetter(84, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // T
    d = max(d, drawLetter(65, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(78, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // N
    d = max(d, drawLetter(83, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // S
    d = max(d, drawLetter(72, p - vec2(6.0 * w, 0.0), vec2(w, size.y))); // H
    return d;
}

float drawString_MASS(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 8.0;
    d = max(d, drawLetter(77, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // M
    d = max(d, drawLetter(65, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(83, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // S
    d = max(d, drawLetter(83, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // S
    d = max(d, drawLetter(61, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // =
    d = max(d, drawLetter(49, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // 1
    d = max(d, drawLetter(46, p - vec2(6.0 * w, 0.0), vec2(w, size.y))); // .
    d = max(d, drawLetter(52, p - vec2(7.0 * w, 0.0), vec2(w, size.y))); // 4
    return d;
}

float drawString_RAD(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 8.0;
    d = max(d, drawLetter(82, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // R
    d = max(d, drawLetter(65, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // A
    d = max(d, drawLetter(68, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // D
    d = max(d, drawLetter(61, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // =
    d = max(d, drawLetter(49, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // 1
    d = max(d, drawLetter(50, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // 2
    d = max(d, drawLetter(75, p - vec2(6.0 * w, 0.0), vec2(w, size.y))); // K
    d = max(d, drawLetter(77, p - vec2(7.0 * w, 0.0), vec2(w, size.y))); // M
    return d;
}

float drawString_FIELD(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 8.0;
    d = max(d, drawLetter(66, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // B
    d = max(d, drawLetter(61, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // =
    d = max(d, drawLetter(49, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // 1
    d = max(d, drawLetter(48, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // 0
    d = max(d, drawLetter(94, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // ^
    d = max(d, drawLetter(49, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // 1
    d = max(d, drawLetter(50, p - vec2(6.0 * w, 0.0), vec2(w, size.y))); // 2
    d = max(d, drawLetter(71, p - vec2(7.0 * w, 0.0), vec2(w, size.y))); // G
    return d;
}

float drawString_EQS(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 3.0;
    d = max(d, drawLetter(69, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // E
    d = max(d, drawLetter(81, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // Q
    d = max(d, drawLetter(83, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // S
    return d;
}

float drawString_EQ_DIPOLE(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 8.0;
    d = max(d, drawLetter(82, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // R
    d = max(d, drawLetter(61, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // =
    d = max(d, drawLetter(82, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // R
    d = max(d, drawLetter(48, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // 0
    d = max(d, drawLetter(83, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // S
    d = max(d, drawLetter(73, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // I
    d = max(d, drawLetter(78, p - vec2(6.0 * w, 0.0), vec2(w, size.y))); // N
    d = max(d, drawLetter(50, p - vec2(7.0 * w, 0.0), vec2(w, size.y))); // 2
    return d;
}

float drawString_EQ_PHASE(vec2 p, vec2 size) {
    float d = 0.0;
    float w = size.x / 7.0;
    d = max(d, drawLetter(84, p - vec2(0.0 * w, 0.0), vec2(w, size.y))); // T
    d = max(d, drawLetter(72, p - vec2(1.0 * w, 0.0), vec2(w, size.y))); // H
    d = max(d, drawLetter(61, p - vec2(2.0 * w, 0.0), vec2(w, size.y))); // =
    d = max(d, drawLetter(87, p - vec2(3.0 * w, 0.0), vec2(w, size.y))); // W
    d = max(d, drawLetter(42, p - vec2(4.0 * w, 0.0), vec2(w, size.y))); // *
    d = max(d, drawLetter(68, p - vec2(5.0 * w, 0.0), vec2(w, size.y))); // D
    d = max(d, drawLetter(84, p - vec2(6.0 * w, 0.0), vec2(w, size.y))); // T
    return d;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;

    vec3 ro = u_camera_pos;
    vec3 ta = vec3(0.0, 0.0, 0.0);

    vec3 w = normalize(ta - ro);
    vec3 up = abs(w.y) > 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 u = normalize(cross(up, w));
    vec3 v = cross(w, u);
    vec3 rd = normalize(uv.x * u + uv.y * v + 1.8 * w);

    vec3 bg_color = vec3(0.0);
    float stars = pow(noise(rd * 120.0), 18.0) * 0.6;
    bg_color += vec3(stars);

    float neb = noise(rd * 2.5 + vec3(0.0, u_time * 0.03, 0.0));
    bg_color += max(0.0, neb) * vec3(0.12, 0.03, 0.25);

    const int MAX_STEPS = 130;
    const float STEP_SIZE = 0.05;
    float t = 1.6;

    vec3 accum_color = vec3(0.0);
    float accum_alpha = 0.0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * t;
        float d = length(p);

        if (d > 4.5) {
            t += STEP_SIZE;
            continue;
        }

        float core_dens = 0.0;
        if (d < 0.22) {
            core_dens = 4.0;
        } else {
            core_dens = exp(-8.0 * (d - 0.22)) * 1.8;
        }
        vec3 c_color = vec3(0.9, 0.95, 1.0);

        float omega = 2.0 * 3.14159265 / u_period;
        float theta = omega * (u_time - d / 5.0);

        vec3 m = vec3(sin(u_inclination) * cos(theta), cos(u_inclination), sin(u_inclination) * sin(theta));
        float cos_beta = dot(normalize(p), m);

        vec3 scroll_dir = m * (cos_beta > 0.0 ? 1.0 : -1.0);
        float n_val = noise(p * 4.5 - scroll_dir * u_time * 7.0);

        float dens_cyan = pow(abs(cos_beta), 110.0) * (1.2 / (0.04 + 0.22 * d * d)) * (0.35 + 0.65 * n_val);
        float dens_magenta = pow(abs(cos_beta), 22.0) * (0.9 / (0.08 + 0.28 * d * d)) * (0.35 + 0.65 * n_val) * 0.5;

        float beam_dens = dens_cyan + dens_magenta;
        vec3 b_color = (dens_cyan * vec3(0.0, 0.86, 1.0) + dens_magenta * vec3(1.0, 0.0, 0.63)) / (beam_dens + 0.001);

        float wave_dens = 0.0;
        vec3 w_color = vec3(0.65, 0.15, 1.0);
        if (u_show_waves != 0) {
            float wave_phase = 2.0 * omega * (u_time - d / 5.0);
            float wave_val = sin(wave_phase);
            float wave_shape = pow(max(0.0, wave_val), 10.0);
            wave_dens = wave_shape * (0.22 / (0.12 + d * d)) * (0.45 + 0.55 * noise(p * 3.0 + u_time));
        }

        // Dipole Magnetic Field Lines
        float mag_line_dens = 0.0;
        vec3 mag_color = vec3(0.0, 0.6, 1.0); // Neon cyan-blue
        float z_m = dot(p, m);
        vec3 p_perp = p - z_m * m;
        float r_perp = length(p_perp);
        if (u_show_magnetic_fields != 0 && r_perp > 0.01) {
            vec3 up_m = abs(m.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
            vec3 u_m = normalize(cross(m, up_m));
            vec3 v_m = cross(m, u_m);
            
            float phi = atan(dot(p, v_m), dot(p, u_m));
            float R0 = (d * d * d) / (r_perp * r_perp);
            float loop_select = sin(R0 * 5.0);
            float line_select = cos(8.0 * phi);
            
            if (R0 > 0.35 && R0 < 4.2 && d > 0.22) {
                float radial_thickness = smoothstep(0.91, 1.0, loop_select);
                float angular_thickness = smoothstep(0.85, 1.0, line_select);
                
                mag_line_dens = radial_thickness * angular_thickness * 1.8;
                mag_line_dens *= exp(-0.6 * d);
                mag_line_dens *= smoothstep(0.22, 0.35, d);
                mag_line_dens *= smoothstep(0.04, 0.2, r_perp);
            }
        }

        float total_dens = core_dens + beam_dens + wave_dens + mag_line_dens;
        if (total_dens > 0.01) {
            vec3 step_color = (core_dens * c_color + beam_dens * b_color + wave_dens * w_color + mag_line_dens * mag_color) / total_dens;

            float alpha = 1.0 - exp(-total_dens * STEP_SIZE * 2.2);
            accum_color += (1.0 - accum_alpha) * step_color * alpha;
            accum_alpha += (1.0 - accum_alpha) * alpha;

            if (accum_alpha >= 0.98) {
                accum_alpha = 1.0;
                break;
            }
        }
        t += STEP_SIZE;
    }

    vec3 final_color = accum_color + (1.0 - accum_alpha) * bg_color;
    final_color = vec3(1.0) - exp(-final_color * 1.6);
    final_color = pow(final_color, vec3(1.0 / 2.2));

    // HUD sidebar rendering is now handled natively via Dear ImGui in the main render loop.

    FragColor = vec4(final_color, 1.0);
}
)";

int main() {

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "3D Volumetric Pulsar Simulation", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    std::cout << "========================================================\n"
              << "            3D VOLUMETRIC PULSAR SIMULATION             \n"
              << "========================================================\n"
              << " CONTROLS:\n"
              << "  - UP/DOWN ARROWS  : Adjust spin period P (smooth)\n"
              << "  - LEFT/RIGHT ARROWS: Adjust magnetic inclination angle (smooth)\n"
              << "  - R KEY           : Toggle Radiation Wavefronts ON/OFF\n"
              << "  - ESC KEY         : Close simulation\n"
              << "========================================================" << std::endl;

    ma_device_config deviceConfig;
    ma_device device;
    bool audioInitialized = false;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 44100;
    deviceConfig.dataCallback      = audio_data_callback;
    deviceConfig.pUserData         = NULL;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        std::cerr << "[AUDIO] Failed to initialize audio playback device." << std::endl;
    } else {
        if (ma_device_start(&device) != MA_SUCCESS) {
            std::cerr << "[AUDIO] Failed to start audio device stream." << std::endl;
            ma_device_uninit(&device);
        } else {
            std::cout << "[AUDIO] Cross-platform audio engine started (44.1kHz stereo)." << std::endl;
            audioInitialized = true;
        }
    }

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkCompileErrors(vertexShader, "VERTEX");

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT");

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkCompileErrors(shaderProgram, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    float vertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };
    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    int loc_resolution = glGetUniformLocation(shaderProgram, "u_resolution");
    int loc_time = glGetUniformLocation(shaderProgram, "u_time");
    int loc_period = glGetUniformLocation(shaderProgram, "u_period");
    int loc_inclination = glGetUniformLocation(shaderProgram, "u_inclination");
    int loc_show_waves = glGetUniformLocation(shaderProgram, "u_show_waves");
    int loc_show_magnetic_fields = glGetUniformLocation(shaderProgram, "u_show_magnetic_fields");
    int loc_show_info = glGetUniformLocation(shaderProgram, "u_show_info");
    int loc_camera_pos = glGetUniformLocation(shaderProgram, "u_camera_pos");

    float lastFrameTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        processInput(window, deltaTime);

        // Auto-orbit camera around the pulsar when not dragging the mouse
        if (!isMousePressed.load()) {
            float yaw = cameraYaw.load() + 0.12f * deltaTime;
            cameraYaw.store(yaw);
        }

        float yaw = cameraYaw.load();
        float pitch = cameraPitch.load();
        float dist = cameraDistance.load();

        float camX = dist * cos(pitch) * sin(yaw);
        float camY = dist * sin(pitch);
        float camZ = dist * cos(pitch) * cos(yaw);

        // Synchronize camera normal vector with the audio thread callback
        float len = sqrt(camX * camX + camY * camY + camZ * camZ);
        cameraNx.store(camX / (len + 1e-9f));
        cameraNy.store(camY / (len + 1e-9f));
        cameraNz.store(camZ / (len + 1e-9f));

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        int displayWidth, displayHeight;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glUniform2f(loc_resolution, (float)displayWidth, (float)displayHeight);
        glUniform1f(loc_time, currentFrameTime);
        glUniform1f(loc_period, period.load());

        glUniform1f(loc_inclination, inclination_deg.load() * 3.14159265f / 180.0f);
        glUniform1i(loc_show_waves, show_waves.load() ? 1 : 0);
        glUniform1i(loc_show_magnetic_fields, show_magnetic_fields.load() ? 1 : 0);
        glUniform1i(loc_show_info, show_info.load() ? 1 : 0);
        glUniform3f(loc_camera_pos, camX, camY, camZ);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // 1. Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Info panel sidebar (toggled via 'I' key)
        if (show_info.load()) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(360, (float)displayHeight - 20), ImGuiCond_Always);
            
            // Modern translucent violet glassmorphic style
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.015f, 0.01f, 0.03f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.1f, 1.0f, 0.6f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
            
            ImGui::Begin("Information Panel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 0.95f, 1.0f, 1.0f), " 3D Volumetric Pulsar Simulation");
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.75f, 0.4f, 1.0f, 1.0f), "What is a Pulsar?");
            ImGui::TextWrapped("A pulsar is a highly magnetized, rapidly rotating neutron star formed from the collapsed core of a massive star. It emits beams of electromagnetic radiation out of its magnetic poles. Because the magnetic axis is tilted relative to the spin axis, these beams sweep through space like a lighthouse, creating periodic pulses of energy.");
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.75f, 0.4f, 1.0f, 1.0f), "Key Astrophysical Characteristics:");
            ImGui::Text("- Radius (R)        : ~10 - 12 km");
            ImGui::Text("- Mass (M)          : ~1.4 Solar Masses");
            ImGui::Text("- Core Density (p)  : ~10^17 kg/m^3 (Nuclear Density)");
            ImGui::Text("- Magnetic Field (B): ~10^12 Gauss (Trillions of Earth's)");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.75f, 0.4f, 1.0f, 1.0f), " How the Program Works");
            ImGui::TextWrapped("This simulator models both the visual dynamics and the radio emissions of a pulsar through real-time graphics rendering and audio synthesis.\n\n"
                               "- Volumetric Raymarching: Casts camera rays step-by-step evaluating core, beam, wave, and magnetic dipole density functions.\n\n"
                               "- Real-Time Audio: Synthesizes high-frequency static noise, deep percussive thumps, and chirps synchronized with the rotating magnetic beam.");

            ImGui::TextColored(ImVec4(0.75f, 0.4f, 1.0f, 1.0f), "Developed By : Daksh Damani & Hutansh Mishra ");

            
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }

        // 3. Persistent corner overlay names/credits
        {
            ImGui::SetNextWindowPos(ImVec2((float)displayWidth - 385, (float)displayHeight - 40), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(375, 30), ImGuiCond_Always);
            ImGui::Begin("Credits Overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
            ImGui::TextColored(ImVec4(0.0f, 0.95f, 1.0f, 0.65f), "Developer: Daksh Damani");
            ImGui::SameLine(190);
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.7f, 0.65f), "Mathsby: Hutansh Mishra");
            ImGui::End();
        }

        // 4. Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    if (audioInitialized) {
        ma_device_uninit(&device);
        std::cout << "[AUDIO] Audio engine closed." << std::endl;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Simulation closed gracefully." << std::endl;
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        bool current = show_waves.load();
        show_waves.store(!current);
        std::cout << "[CONTROL] Radiation Wavefronts: " << (!current ? "ON" : "OFF") << std::endl;
    }
    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        bool current = show_magnetic_fields.load();
        show_magnetic_fields.store(!current);
        std::cout << "[CONTROL] Magnetic Field Lines: " << (!current ? "ON" : "OFF") << std::endl;
    }
    if (key == GLFW_KEY_I && action == GLFW_PRESS) {
        bool current = show_info.load();
        show_info.store(!current);
        if (!current) {
            std::cout << "\n========================================================\n"
                      << "             🌌 PULSAR SIMULATION INFO PANEL 🌌            \n"
                      << "========================================================\n"
                      << " ABOUT PULSARS:\n"
                      << "  A pulsar is a highly magnetized, rapidly rotating neutron star\n"
                      << "  formed from the collapsed core of a massive star. It emits\n"
                      << "  beams of electromagnetic radiation out of its magnetic poles.\n"
                      << "  These beams sweep through space like a lighthouse, creating\n"
                      << "  periodic pulses of energy detected on Earth.\n\n"
                      << " ASTROPHYSICAL CHARACTERISTICS:\n"
                      << "  - Radius (R)       : ~10 - 12 km\n"
                      << "  - Mass (M)         : ~1.4 Solar Masses (Chandrasekhar Limit)\n"
                      << "  - Core Density (p) : ~10^17 kg/m^3 (Nuclear Density)\n"
                      << "  - Magnetic Field(B): ~10^12 Gauss (Trillions of times Earth's)\n\n"
                      << " MATHEMATICAL & PHYSICS EQUATIONS:\n"
                      << "  1. Spin Angular Velocity: \n"
                      << "     Omega = 2 * pi / P (P is spin period)\n"
                      << "  2. Dipole Magnetic Field Lines: \n"
                      << "     r = R0 * sin^2(theta) \n"
                      << "  3. Retarded Rotation Phase (Speed of Light Delay c = 5.0): \n"
                      << "     theta = Omega * (t - r / c) \n"
                      << "  4. Lighthouse Alignment / Signal Amplitude: \n"
                      << "     cos(beta) = v_camera . m_magnetic\n\n"
                      << " PROJECT CREDITS:\n"
                      << "  - Developer                  : Daksh Damani\n"
                      << "  - Mathematics & Astrophysics : Hutansh Mishra\n\n"
                      << " ABOUT THIS PROJECT:\n"
                      << "  - Graphics: GPU-accelerated volumetric raymarching in GLSL.\n"
                      << "  - Audio: Real-time procedural synthesis synchronized with\n"
                      << "           the rotating magnetic beam using Miniaudio.\n"
                      << "  - Orbit: Free 360-degree rotation using mouse dragging.\n\n"
                      << " CONTROLS:\n"
                      << "  [Mouse Drag] : Rotate camera 360 degrees freely\n"
                      << "  [Scroll Wheel]: Zoom camera in and out\n"
                      << "  [UP/DOWN]    : Adjust spin period (slower/faster)\n"
                      << "  [LEFT/RIGHT] : Adjust magnetic inclination angle\n"
                      << "  [M] Key      : Toggle Dipole Magnetic Field Lines ON/OFF\n"
                      << "  [R] Key      : Toggle Radiation Wavefronts ON/OFF\n"
                      << "  [I] Key      : Toggle Info Overlay ON/OFF\n"
                      << "  [ESC]        : Exit Simulation\n"
                      << "========================================================\n" << std::endl;
        } else {
            std::cout << "[CONTROL] Info Panel: OFF" << std::endl;
        }
    }
}

void processInput(GLFWwindow* window, float deltaTime) {
    bool stateChanged = false;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        float current = period.load();
        current += 0.8f * deltaTime;
        if (current > 10.0f) current = 10.0f;
        period.store(current);
        stateChanged = true;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        float current = period.load();
        current -= 0.8f * deltaTime;
        if (current < 0.1f) current = 0.1f;
        period.store(current);
        stateChanged = true;
    }

    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        float current = inclination_deg.load();
        current += 20.0f * deltaTime;
        if (current > 90.0f) current = 90.0f;
        inclination_deg.store(current);
        stateChanged = true;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        float current = inclination_deg.load();
        current -= 20.0f * deltaTime;
        if (current < 0.0f) current = 0.0f;
        inclination_deg.store(current);
        stateChanged = true;
    }

    if (stateChanged) {
        static double lastPrintTime = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPrintTime > 0.08) {
            std::cout << "[STATUS] Period (P): " << std::fixed << std::setprecision(2) << period.load()
                      << "s | Inclination (alpha): " << inclination_deg.load() << " deg" << std::endl;
            lastPrintTime = currentTime;
        }
    }
}

void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* pOutFloat = (float*)pOutput;

    static double localTime = 0.0;
    const double sampleRate = (double)pDevice->sampleRate;
    const double dt = 1.0 / sampleRate;

    static double carrierPhase = 0.0;
    static double thumpPhase = 0.0;
    static double humPhase = 0.0;

    static bool seeded = false;
    if (!seeded) {
        srand(12345);
        seeded = true;
    }

    for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {

        // Synchronize with the user's manual/auto rotation of the camera
        double camNx = (double)cameraNx.load();
        double camNy = (double)cameraNy.load();
        double camNz = (double)cameraNz.load();

        double currentPeriod = (double)period.load();
        double currentInclinationDeg = (double)inclination_deg.load();
        double currentInclination = currentInclinationDeg * 3.1415926535897932 / 180.0;

        double omega = 2.0 * 3.1415926535897932 / currentPeriod;
        double theta = omega * (localTime - (double)cameraDistance.load() / 5.0);

        double mX = sin(currentInclination) * cos(theta);
        double mY = cos(currentInclination);
        double mZ = sin(currentInclination) * sin(theta);

        double cosBeta = camNx * mX + camNy * mY + camNz * mZ;
        double absCosBeta = abs(cosBeta);

        // 1. Radio static noise burst (high-frequency cosmic hiss)
        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float noiseIntensity = (float)pow(absCosBeta, 150.0);
        float noiseSample = noise * 0.45f * noiseIntensity;

        // 2. Deep percussive thumping (simulating the heartbeat/pulse structure)
        thumpPhase += 2.0 * 3.1415926535897932 * 55.0 * dt;
        if (thumpPhase > 2.0 * 3.1415926535897932) thumpPhase -= 2.0 * 3.1415926535897932;
        float thumpIntensity = (float)pow(absCosBeta, 250.0);
        float thumpSample = (float)sin(thumpPhase) * 0.65f * thumpIntensity;

        // 3. Carrier synth tone (sci-fi sweep chirp)
        double freq = 180.0 + 450.0 * pow(absCosBeta, 120.0);
        carrierPhase += 2.0 * 3.1415926535897932 * freq * dt;
        if (carrierPhase > 2.0 * 3.1415926535897932) carrierPhase -= 2.0 * 3.1415926535897932;
        float chirpSample = (float)sin(carrierPhase) * 0.18f * (float)pow(absCosBeta, 120.0);

        // 4. Background spin rotation hum (provides pitch-feedback on period modifications)
        double humFreq = 8.0 * (1.0 / currentPeriod);
        if (humFreq < 20.0) humFreq = 20.0;
        if (humFreq > 300.0) humFreq = 300.0;
        humPhase += 2.0 * 3.1415926535897932 * humFreq * dt;
        if (humPhase > 2.0 * 3.1415926535897932) humPhase -= 2.0 * 3.1415926535897932;
        float humSample = (float)sin(humPhase) * 0.05f * (0.8f + 0.2f * (float)cosBeta);

        float sample = (noiseSample + thumpSample + chirpSample + humSample) * 0.16f;

        pOutFloat[iFrame * 2]     = sample;
        pOutFloat[iFrame * 2 + 1] = sample;

        localTime += dt;
    }
}

void checkCompileErrors(unsigned int shader, std::string type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                      << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                      << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

double lastX = 640.0;
double lastY = 360.0;
bool firstMouse = true;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            isMousePressed.store(true);
            firstMouse = true;
        } else if (action == GLFW_RELEASE) {
            isMousePressed.store(false);
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!isMousePressed.load()) return;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos); // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    // Adjust horizontal and vertical orbit angles
    float sensitivity = 0.005f;
    float yaw = cameraYaw.load() - xoffset * sensitivity;
    float pitch = cameraPitch.load() - yoffset * sensitivity;

    // Clamp pitch to avoid gimbal lock
    if (pitch > 1.52f)  pitch = 1.52f;
    if (pitch < -1.52f) pitch = -1.52f;

    cameraYaw.store(yaw);
    cameraPitch.store(pitch);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float dist = cameraDistance.load() - (float)yoffset * 0.3f;
    if (dist < 2.0f) dist = 2.0f;
    if (dist > 10.0f) dist = 10.0f;
    cameraDistance.store(dist);
}

