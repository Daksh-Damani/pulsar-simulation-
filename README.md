# 🌌 3D Volumetric Pulsar Simulation

An interactive, high-performance, real-time 3D simulation of a **pulsar** utilizing GPU-accelerated volumetric raymarching for graphics and procedural synthesis for synchronized stereo audio. Built in C++17 using OpenGL (GLSL 3.3 Core), GLFW, GLAD, and the single-header `miniaudio` library.

---

## 🪐 What is a Pulsar?

A **pulsar** (short for *pulsating star*) is a highly magnetized, rapidly rotating neutron star formed from the collapsed core of a massive star during a supernova explosion. 

```
          Spin Axis (z)
              ▲
              │   / Magnetic Axis (m)
              │  /
              │α/   Inclination Angle (alpha)
              │/ 
      ┌───────┼───────┐
      │       │       │
      │   ┌───┴───┐   │  ◄── Neutron Star Core
      │   │       │   │
      │   └───┬───┘   │
      │       │       │
      └───────┼───────┘
             /│
            / │
           /  │
              ▼
```

### Key Astrophysical Characteristics:
* **Extreme Density:** A neutron star packs the mass of about 1.4 Suns into a sphere only 20 kilometers (12 miles) in diameter.
* **Rapid Rotation:** Due to the conservation of angular momentum during collapse, pulsars rotate at mind-boggling speeds—from once every few seconds down to hundreds of rotations per second (millisecond pulsars).
* **Intense Magnetic Fields:** Their magnetic fields are trillions of times stronger than Earth's.
* **The Lighthouse Effect:** Pulsars emit powerful, highly collimated beams of electromagnetic radiation along their magnetic poles. Because the magnetic axis is tilted relative to the spin axis by an inclination angle ($\alpha$), these beams sweep through space like a lighthouse. As the beam crosses our line of sight (Earth), we detect periodic, extremely precise pulses of radiation.

---

## 💻 How the Program Works

This simulator models both the visual dynamics and the radio emissions of a pulsar through real-time graphics rendering and audio synthesis.

### 1. Volumetric Raymarching (GPU Graphics)
Instead of drawing traditional 3D polygon meshes, the program renders the pulsar using **volumetric raymarching** entirely inside a GLSL 3.3 Core fragment shader. 
* **Look-At Camera Matrix:** The camera orbits slowly around the pulsar to provide a dynamic 3D perspective of the system.
* **Raymarching Loop:** For every pixel, a camera ray is cast into the 3D scene. The shader marches along the ray step-by-step, evaluating three primary density functions:
  1. **Stellar Core:** A bright ice-blue spherical core at the center, modeled with an exponential fall-off function.
  2. **Conical Emission Beams:** Tilted relative to the rotation axis by inclination angle $u\_inclination$, rotating based on the spin period $u\_period$. The plasma flow is animated dynamically using procedural 3D value noise.
  3. **Radiation Wavefronts:** Concentric spherical shells propagating outward at the speed of light, representing emitted radio waves.
* **Procedural Nebula and Space:** The cosmic backdrop features a procedural starfield and swirling gas clouds generated via a GPU-based Jenkins hash noise algorithm.

### 2. Real-Time Procedural Audio Synthesis
To simulate the radio pulses detected by radio telescopes, the program synthesizes stereo audio on a separate thread in real-time.
* **Perfect Synchronization:** The audio thread tracks the camera's path and the pulsar's rotating magnetic axis using the exact mathematical formulas as the GLSL fragment shader. It calculates the dot product between the camera vector and the magnetic axis:
  $$\cos(\beta) = \vec{v}_{\text{camera}} \cdot \vec{m}_{\text{magnetic}}$$
* **Signal Synthesis:** When the beam aligns with the camera ($\cos(\beta) \to 1$), the audio callback triggers a synthesized burst.
* **Chirp & Crackle Modulation:** The pulse consists of:
  * **White Noise:** Simulating the crackle of interstellar radio static.
  * **Sweeping Carrier Wave:** A sine wave whose frequency chirps from $300\text{ Hz}$ to $850\text{ Hz}$ based on the alignment intensity, creating a classic sci-fi "sweep" effect.

### 3. Thread-Safe Input Handling
The spin period ($P$) and magnetic inclination ($\alpha$) are stored in atomic variables (`std::atomic<float>`). This ensures that changes made by user keyboard input on the main application thread are instantly and safely reflected in both the GPU rendering uniforms and the real-time audio synthesis thread without locking.

---

## ⌨️ Controls

All parameters can be adjusted interactively in real-time while the simulation is running:

| Input | Action | Bounds / Behavior |
| :--- | :--- | :--- |
| `▲ UP` Arrow | **Increase spin period ($P$)** | Clamped at max $10.0\text{ seconds}$ (slower rotation) |
| `▼ DOWN` Arrow | **Decrease spin period ($P$)** | Clamped at min $0.1\text{ seconds}$ (extremely fast rotation) |
| `► RIGHT` Arrow | **Increase magnetic inclination ($\alpha$)** | Clamped at max $90.0^\circ$ (beams rotate horizontally) |
| `◄ LEFT` Arrow | **Decrease magnetic inclination ($\alpha$)** | Clamped at min $0.0^\circ$ (beams align with the spin axis) |
| `M` Key | **Toggle Magnetic Field Lines** | Toggles the 3D procedural magnetic dipole field loops |
| `R` Key | **Toggle Radiation Wavefronts** | Toggles the expanding purple radio emission shells |
| `I` Key | **Toggle Information Sidebar** | Toggles the on-screen Dear ImGui science overlay panel |
| `Mouse Drag` | **Orbit Camera** | Free 360-degree interactive camera rotation |
| `Mouse Scroll` | **Zoom Camera** | Zoom the camera viewport in and out |
| `ESC` Key | **Exit Simulation** | Closes the window and shuts down the audio engine cleanly |

---

## 🛠️ Build and Installation Guide

This project uses **CMake** and C++17. All major dependencies (GLFW, GLAD, Dear ImGui, and miniaudio) are automatically downloaded, compiled, and linked during the build configuration using CMake's `FetchContent` module.

### Prerequisites

You must have a C++17-capable compiler, Git, and CMake installed on your system.

---

### 🐧 Linux (Debian / Ubuntu)

On Linux, you must install the development packages for OpenGL, X11 (for GLFW windowing), and ALSA (for miniaudio sound playback).

#### 1. Install Dependencies
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential \
                       libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
                       libgl1-mesa-dev libasound2-dev
```

#### 2. Configure and Build
```bash
# From the project root directory
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

#### 3. Run
```bash
./pulsar_sim
```

---

### 🍎 macOS

On macOS, Xcode Command Line Tools are required. All necessary windowing frameworks (Cocoa, IOKit, CoreVideo) and OpenGL dependencies are bundled with macOS and configured automatically.

#### 1. Install Build Tools
If you don't have Xcode Command Line Tools or CMake installed:
```bash
xcode-select --install
brew install cmake
```

#### 2. Configure and Build
```bash
# From the project root directory
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

#### 3. Run
```bash
./pulsar_sim
```

---

### 🪟 Windows

#### 1. Install Prerequisites
You can install all necessary tools using the Windows Package Manager (`winget`) in Command Prompt (CMD) or PowerShell:

```cmd
:: Install Git and CMake
winget install -e --id Git.Git
winget install -e --id Kitware.CMake

:: Install Visual Studio 2022 Community with C++ build tools
winget install -e --id Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"
```
> [!NOTE]
> You may need to restart your terminal after installation for the new environment paths to load.

#### 2. Configure and Build via Command Line

##### Option A: Command Prompt (CMD)
```cmd
:: Create and navigate to build directory
mkdir build
cd build

:: Configure the project
cmake ..

:: Compile the release binary
cmake --build . --config Release

:: Run the simulation
Release\pulsar_sim.exe
```

##### Option B: PowerShell
```powershell
# Create and navigate to build directory
New-Item -ItemType Directory -Force -Path .\build
cd .\build

# Configure the project
cmake ..

# Compile the release binary
cmake --build . --config Release

# Run the simulation
.\Release\pulsar_sim.exe
```

##### Option C: Git Bash
```bash
# Create and navigate to build directory
mkdir build
cd build

# Configure the project
cmake ..

# Compile the release binary
cmake --build . --config Release

# Run the simulation
./Release/pulsar_sim.exe
```

#### 3. Alternative: Using Visual Studio IDE
1. Open **Visual Studio**.
2. Select **Open a local folder** and choose the root folder of this project (which contains `CMakeLists.txt`).
3. Visual Studio will automatically detect CMake and configure the project.
4. Select `pulsar_sim.exe` from the startup item dropdown.
5. Press **Ctrl + F5** (or click the green Play button) to build and run the simulation.

