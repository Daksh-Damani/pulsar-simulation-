#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cmath>
#include <iomanip>
#include <atomic>
#include <cstdlib>

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
        float theta = omega * u_time - 1.25 * d;

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
            float wave_phase = d * 6.5 - omega * u_time * 2.0;
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

    // Glassmorphic HUD overlay
    if (u_show_info != 0) {
        vec2 hud_uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
        vec2 center = vec2(0.0, 0.0);
        vec2 size = vec2(0.55, 0.38);
        vec2 d_hud = abs(hud_uv - center) - size;
        float card_dist = max(d_hud.x, d_hud.y);
        
        if (card_dist < 0.0) {
            vec3 card_bg = vec3(0.01, 0.03, 0.08);
            float border_val = 1.0 - smoothstep(0.0, 0.003, abs(card_dist));
            vec3 border_color = vec3(0.0, 0.75, 1.0);
            
            vec2 grid = abs(fract(hud_uv * 18.0 - 0.5) - 0.5) / 18.0;
            float grid_line = min(grid.x, grid.y);
            float grid_val = 1.0 - smoothstep(0.0, 0.0015, grid_line);
            
            final_color = mix(final_color * 0.12 + card_bg, border_color, border_val);
            final_color += grid_val * vec3(0.0, 0.25, 0.5) * 0.15;
            
            // Progress Bar 1: Rotation Period
            vec2 bar1_size = vec2(0.3, 0.012);
            vec2 bar1_pos = vec2(0.0, 0.08);
            vec2 d_bar1 = abs(hud_uv - bar1_pos) - bar1_size;
            float dist_bar1 = max(d_bar1.x, d_bar1.y);
            
            if (dist_bar1 < 0.0) {
                final_color = mix(final_color, vec3(0.06, 0.1, 0.18), 0.75);
                float fill_ratio = (u_period - 0.1) / (10.0 - 0.1);
                float x_local = (hud_uv.x - (bar1_pos.x - bar1_size.x)) / (2.0 * bar1_size.x);
                if (x_local < fill_ratio) {
                    final_color = mix(final_color, vec3(0.0, 0.85, 1.0), 0.9);
                }
            } else if (abs(dist_bar1) < 0.002) {
                final_color = vec3(0.0, 0.8, 1.0);
            }
            
            // Progress Bar 2: Magnetic Inclination
            vec2 bar2_size = vec2(0.3, 0.012);
            vec2 bar2_pos = vec2(0.0, -0.06);
            vec2 d_bar2 = abs(hud_uv - bar2_pos) - bar2_size;
            float dist_bar2 = max(d_bar2.x, d_bar2.y);
            
            if (dist_bar2 < 0.0) {
                final_color = mix(final_color, vec3(0.06, 0.1, 0.18), 0.75);
                float fill_ratio = u_inclination / (90.0 * 3.14159265 / 180.0);
                float x_local = (hud_uv.x - (bar2_pos.x - bar2_size.x)) / (2.0 * bar2_size.x);
                if (x_local < fill_ratio) {
                    final_color = mix(final_color, vec3(1.0, 0.0, 0.6), 0.9);
                }
            } else if (abs(dist_bar2) < 0.002) {
                final_color = vec3(1.0, 0.0, 0.6);
            }
            
            // Status Indicator Dots
            vec2 dot1_pos = vec2(-0.15, -0.18);
            vec2 dot2_pos = vec2(0.15, -0.18);
            
            float d_dot1 = length(hud_uv - dot1_pos) - 0.015;
            if (d_dot1 < 0.0) {
                vec3 col = (u_show_magnetic_fields != 0) ? vec3(0.0, 1.0, 0.4) : vec3(0.5, 0.5, 0.5);
                final_color = mix(final_color, col, 0.95);
            } else if (abs(d_dot1) < 0.0015) {
                final_color = vec3(1.0);
            }
            
            float d_dot2 = length(hud_uv - dot2_pos) - 0.015;
            if (d_dot2 < 0.0) {
                vec3 col = (u_show_waves != 0) ? vec3(0.0, 1.0, 0.4) : vec3(0.5, 0.5, 0.5);
                final_color = mix(final_color, col, 0.95);
            } else if (abs(d_dot2) < 0.0015) {
                final_color = vec3(1.0);
            }
        }
    }

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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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
        double theta = omega * localTime - 1.25 * 4.8;

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

