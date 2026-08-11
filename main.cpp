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

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void checkCompileErrors(unsigned int shader, std::string type);
void processInput(GLFWwindow* window, float deltaTime);
void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

// Global state variables (Thread-safe atomic variables shared with audio thread)
std::atomic<float> period{1.0f};             // Spin period P in seconds (Default: 1.0s)
std::atomic<float> inclination_deg{30.0f};   // Magnetic inclination angle alpha in degrees (Default: 30 deg)
std::atomic<bool> show_waves{true};          // Toggle radiation wavefront propagation

// Embedded Shader Sources (GLSL 330 Core)
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
uniform float u_inclination; // in radians
uniform int u_show_waves;

// A single iteration of Bob Jenkins' One-at-a-time hash
float hash(vec3 p) {
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p.xyz, p.yzx + 19.19);
    return fract(p.x * p.y * p.z);
}

// 3D value noise running entirely on GPU
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
    // Normalize coordinates with aspect ratio correction
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
    
    // Orbit camera slowly around the pulsar for a premium 3D perspective
    float camAngle = u_time * 0.15;
    vec3 ro = vec3(4.8 * sin(camAngle), 2.2 * cos(camAngle * 0.4), 4.8 * cos(camAngle));
    vec3 ta = vec3(0.0, 0.0, 0.0); // look at center
    
    // Look-at camera matrix
    vec3 w = normalize(ta - ro);
    vec3 u = normalize(cross(vec3(0.0, 1.0, 0.0), w));
    vec3 v = cross(w, u);
    vec3 rd = normalize(uv.x * u + uv.y * v + 1.8 * w);
    
    // Starfield background
    vec3 bg_color = vec3(0.0);
    float stars = pow(noise(rd * 120.0), 18.0) * 0.6;
    bg_color += vec3(stars);
    
    // Soft, swirling interstellar gas glow
    float neb = noise(rd * 2.5 + vec3(0.0, u_time * 0.03, 0.0));
    bg_color += max(0.0, neb) * vec3(0.12, 0.03, 0.25);
    
    // Raymarching settings
    const int MAX_STEPS = 130;
    const float STEP_SIZE = 0.05;
    float t = 1.6; // Start offset to skip empty foreground space
    
    vec3 accum_color = vec3(0.0);
    float accum_alpha = 0.0;
    
    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * t;
        float d = length(p);
        
        // Skip steps outside our active simulation radius (4.5 units)
        if (d > 4.5) {
            t += STEP_SIZE;
            continue;
        }
        
        // 1. Stellar Core (Neutron Star Sphere)
        float core_dens = 0.0;
        if (d < 0.22) {
            core_dens = 4.0;
        } else {
            core_dens = exp(-8.0 * (d - 0.22)) * 1.8;
        }
        vec3 c_color = vec3(0.9, 0.95, 1.0); // Brilliant ice blue-white
        
        // 2. Conical Emission Beams
        // Omega (angular velocity) = 2PI / P
        float omega = 2.0 * 3.14159265 / u_period;
        // theta_rot with propagation delay (relativisitic garden hose spiral)
        float theta = omega * u_time - 1.25 * d; 
        
        vec3 m = vec3(sin(u_inclination) * cos(theta), cos(u_inclination), sin(u_inclination) * sin(theta));
        float cos_beta = dot(normalize(p), m);
        
        // Outward plasma flow noise mapping
        vec3 scroll_dir = m * (cos_beta > 0.0 ? 1.0 : -1.0);
        float n_val = noise(p * 4.5 - scroll_dir * u_time * 7.0);
        
        // Vivid cyan core beam + high-intensity magenta sheath
        float dens_cyan = pow(abs(cos_beta), 110.0) * (1.2 / (0.04 + 0.22 * d * d)) * (0.35 + 0.65 * n_val);
        float dens_magenta = pow(abs(cos_beta), 22.0) * (0.9 / (0.08 + 0.28 * d * d)) * (0.35 + 0.65 * n_val) * 0.5;
        
        float beam_dens = dens_cyan + dens_magenta;
        vec3 b_color = (dens_cyan * vec3(0.0, 0.86, 1.0) + dens_magenta * vec3(1.0, 0.0, 0.63)) / (beam_dens + 0.001);
        
        // 3. Radiation Wavefronts
        float wave_dens = 0.0;
        vec3 w_color = vec3(0.65, 0.15, 1.0); // Purple-magenta wavefronts
        if (u_show_waves != 0) {
            // Wavefronts propagating outward
            float wave_phase = d * 6.5 - omega * u_time * 2.0;
            float wave_val = sin(wave_phase);
            // Sharp shells via power function
            float wave_shape = pow(max(0.0, wave_val), 10.0);
            wave_dens = wave_shape * (0.22 / (0.12 + d * d)) * (0.45 + 0.55 * noise(p * 3.0 + u_time));
        }
        
        // Standard front-to-back alpha blending
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
    
    // Blend accumulated color with space background
    vec3 final_color = accum_color + (1.0 - accum_alpha) * bg_color;
    
    // Exposure tone mapping
    final_color = vec3(1.0) - exp(-final_color * 1.6);
    
    // Gamma correction
    final_color = pow(final_color, vec3(1.0 / 2.2));
    
    FragColor = vec4(final_color, 1.0);
}
)";

int main() {
    // 1. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set GLFW window options for OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS
#endif
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    // Create 1280x720 window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "3D Volumetric Pulsar Simulation", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    // 2. Load GLAD function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Display controls instructions in the console
    std::cout << "========================================================\n"
              << "            3D VOLUMETRIC PULSAR SIMULATION             \n"
              << "========================================================\n"
              << " CONTROLS:\n"
              << "  - UP/DOWN ARROWS  : Adjust spin period P (smooth)\n"
              << "  - LEFT/RIGHT ARROWS: Adjust magnetic inclination angle (smooth)\n"
              << "  - R KEY           : Toggle Radiation Wavefronts ON/OFF\n"
              << "  - ESC KEY         : Close simulation\n"
              << "========================================================" << std::endl;

    // 3. Initialize Audio Subsystem (miniaudio)
    ma_device_config deviceConfig;
    ma_device device;
    bool audioInitialized = false;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32; // Floating point samples
    deviceConfig.playback.channels = 2;             // Stereo output
    deviceConfig.sampleRate        = 44100;         // CD quality
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

    // 4. Compile and Link Shaders
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

    // Clean up individual shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 5. Setup Quad Vertices (2 triangles for full-screen rendering)
    float vertices[] = {
        -1.0f,  1.0f,  // Top Left
        -1.0f, -1.0f,  // Bottom Left
         1.0f, -1.0f,  // Bottom Right
         1.0f,  1.0f   // Top Right
    };
    unsigned int indices[] = {
        0, 1, 2,  // First Triangle
        0, 2, 3   // Second Triangle
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

    // Vertex positions attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Retrieve uniform locations
    int loc_resolution = glGetUniformLocation(shaderProgram, "u_resolution");
    int loc_time = glGetUniformLocation(shaderProgram, "u_time");
    int loc_period = glGetUniformLocation(shaderProgram, "u_period");
    int loc_inclination = glGetUniformLocation(shaderProgram, "u_inclination");
    int loc_show_waves = glGetUniformLocation(shaderProgram, "u_show_waves");

    // Timing tracking
    float lastFrameTime = 0.0f;

    // 6. Simulation Loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // Process arrow keys continuously for smooth transitions
        processInput(window, deltaTime);

        // Render pass
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Bind shader program and update uniform values
        glUseProgram(shaderProgram);

        // Get window framebuffer resolution (respecting DPI scaling)
        int displayWidth, displayHeight;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glUniform2f(loc_resolution, (float)displayWidth, (float)displayHeight);
        glUniform1f(loc_time, currentFrameTime);
        glUniform1f(loc_period, period.load());
        // Convert inclination to radians before sending to GPU
        glUniform1f(loc_inclination, inclination_deg.load() * 3.14159265f / 180.0f);
        glUniform1i(loc_show_waves, show_waves.load() ? 1 : 0);

        // Draw the full-screen quad
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Swap buffers and handle window event polling
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup resources
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    // Cleanup audio
    if (audioInitialized) {
        ma_device_uninit(&device);
        std::cout << "[AUDIO] Audio engine closed." << std::endl;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Simulation closed gracefully." << std::endl;
    return 0;
}

// GLFW Resize Callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// GLFW Keyboard Event Callback (Toggles & Immediate closes)
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

// Process continuous keys in loop for smooth value adjustment
void processInput(GLFWwindow* window, float deltaTime) {
    bool stateChanged = false;

    // Smoothly adjust spin period P (Up / Down)
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        float current = period.load();
        current += 0.8f * deltaTime; // Adjust speed
        if (current > 10.0f) current = 10.0f; // Clamp to 10.0 seconds
        period.store(current);
        stateChanged = true;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        float current = period.load();
        current -= 0.8f * deltaTime;
        if (current < 0.1f) current = 0.1f; // Clamp to 0.1 seconds to avoid division by zero
        period.store(current);
        stateChanged = true;
    }

    // Smoothly adjust inclination angle alpha (Left / Right)
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        float current = inclination_deg.load();
        current += 20.0f * deltaTime; // 20 degrees per second
        if (current > 90.0f) current = 90.0f; // Clamp to 90 degrees
        inclination_deg.store(current);
        stateChanged = true;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        float current = inclination_deg.load();
        current -= 20.0f * deltaTime;
        if (current < 0.0f) current = 0.0f; // Clamp to 0 degrees
        inclination_deg.store(current);
        stateChanged = true;
    }

    // Output adjustments to the console in real-time, rate-limited for clean terminal output
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

// Procedural Audio Callback Synthesizer
void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* pOutFloat = (float*)pOutput;
    
    // Track local time and carrier phase on the audio thread
    static double localTime = 0.0;
    const double sampleRate = (double)pDevice->sampleRate;
    const double dt = 1.0 / sampleRate;
    
    static double carrierPhase = 0.0;
    
    // Seed standard rand() once if needed
    static bool seeded = false;
    if (!seeded) {
        srand(12345);
        seeded = true;
    }

    for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
        // 1. Calculate camera position at this localTime (exactly matching the shader camera path)
        double camAngle = localTime * 0.15;
        double camX = sin(camAngle);
        double camY = cos(camAngle * 0.4);
        double camZ = cos(camAngle);
        // Normalize camera vector
        double camLen = sqrt(camX*camX + camY*camY + camZ*camZ);
        double camNx = camX / (camLen + 1e-9);
        double camNy = camY / (camLen + 1e-9);
        double camNz = camZ / (camLen + 1e-9);
        
        // 2. Calculate magnetic axis at this localTime at the camera distance (d = 4.8)
        double currentPeriod = (double)period.load();
        double currentInclinationDeg = (double)inclination_deg.load();
        double currentInclination = currentInclinationDeg * 3.1415926535897932 / 180.0;
        
        double omega = 2.0 * 3.1415926535897932 / currentPeriod;
        // theta_rot with propagation delay (d = 4.8, matching the shader)
        double theta = omega * localTime - 1.25 * 4.8;
        
        double mX = sin(currentInclination) * cos(theta);
        double mY = cos(currentInclination);
        double mZ = sin(currentInclination) * sin(theta);
        
        // 3. Dot product (alignment between camera and magnetic axis)
        double cosBeta = camNx * mX + camNy * mY + camNz * mZ;
        
        // 4. Beam intensity (very high power of alignment, matching the shader pow(abs(cos_beta), 110.0))
        double absCosBeta = abs(cosBeta);
        double beamIntensity = pow(absCosBeta, 110.0);
        
        // 5. Generate noise for static radio burst
        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        
        // 6. Generate sine carrier wave (sweeping chirp pitch based on beam intensity)
        // Whenever the beam sweeps, the frequency chirps from 300Hz to 850Hz
        double freq = 300.0 + 550.0 * beamIntensity;
        carrierPhase += 2.0 * 3.1415926535897932 * freq * dt;
        if (carrierPhase > 2.0 * 3.1415926535897932) {
            carrierPhase = fmod(carrierPhase, 2.0 * 3.1415926535897932);
        }
        float sineVal = (float)sin(carrierPhase);
        
        // 7. Synthesize sound pulse (mix static noise and chirp)
        // Clean radio static crackle + sci-fi beeping chirp
        float pulse = (0.75f * noise + 0.25f * sineVal) * (float)beamIntensity;
        
        // Apply comfortable volume damping
        float sample = pulse * 0.12f; 
        
        // Write to stereo output channels
        pOutFloat[iFrame * 2]     = sample; // Left
        pOutFloat[iFrame * 2 + 1] = sample; // Right
        
        localTime += dt;
    }
}

// OpenGL Shader compile helper
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
