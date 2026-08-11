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
void checkCompileErrors(unsigned int shader, std::string type);
void processInput(GLFWwindow* window, float deltaTime);
void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

std::atomic<float> period{1.0f};
std::atomic<float> inclination_deg{30.0f};
std::atomic<bool> show_waves{true};

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

    float camAngle = u_time * 0.15;
    vec3 ro = vec3(4.8 * sin(camAngle), 2.2 * cos(camAngle * 0.4), 4.8 * cos(camAngle));
    vec3 ta = vec3(0.0, 0.0, 0.0);

    vec3 w = normalize(ta - ro);
    vec3 u = normalize(cross(vec3(0.0, 1.0, 0.0), w));
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

        float total_dens = core_dens + beam_dens + wave_dens;
        if (total_dens > 0.01) {
            vec3 step_color = (core_dens * c_color + beam_dens * b_color + wave_dens * w_color) / total_dens;

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

    float lastFrameTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        processInput(window, deltaTime);

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

    static bool seeded = false;
    if (!seeded) {
        srand(12345);
        seeded = true;
    }

    for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {

        double camAngle = localTime * 0.15;
        double camX = sin(camAngle);
        double camY = cos(camAngle * 0.4);
        double camZ = cos(camAngle);

        double camLen = sqrt(camX*camX + camY*camY + camZ*camZ);
        double camNx = camX / (camLen + 1e-9);
        double camNy = camY / (camLen + 1e-9);
        double camNz = camZ / (camLen + 1e-9);

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
        double beamIntensity = pow(absCosBeta, 110.0);

        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;

        double freq = 300.0 + 550.0 * beamIntensity;
        carrierPhase += 2.0 * 3.1415926535897932 * freq * dt;
        if (carrierPhase > 2.0 * 3.1415926535897932) {
            carrierPhase = fmod(carrierPhase, 2.0 * 3.1415926535897932);
        }
        float sineVal = (float)sin(carrierPhase);

        float pulse = (0.75f * noise + 0.25f * sineVal) * (float)beamIntensity;

        float sample = pulse * 0.12f;

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

