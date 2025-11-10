# Audio Loop Station

Real-time multi-track audio looping application for live performance and music creation.

## Overview

Audio Loop Station enables musicians to record, layer, and manipulate up to four audio tracks in synchronized loops. Built with C++ and the JUCE framework for professional-grade audio quality and cross-platform compatibility.

**Key Features:**
- Real-time recording and playback with <10ms latency
- 4 independent tracks with individual volume and pan controls
- Sample-accurate synchronization across all tracks
- Multiple loop management for song sections
- Cross-platform support (Windows, macOS, Linux)

## Prerequisites

Before building, install the following:

### Required Software

1. **JUCE Framework** (v7.0+)  
   Download: https://juce.com/download/

2. **CMake** (v3.15+)  
   Download: https://cmake.org/download/

3. **C++17 Compatible Compiler**
    - Windows: Visual Studio 2019 or newer
    - macOS: Xcode 12 or newer
    - Linux: GCC 9+ or Clang 10+

### Recommended IDE

**CLion** (Free for students via JetBrains Student Pack)  
Download: https://www.jetbrains.com/clion/  
Student Pack: https://www.jetbrains.com/academy/student-pack/

### Platform-Specific Requirements

#### Windows
- Visual Studio 2019 or newer (Community Edition works)
- Windows SDK 10.0 or newer
- Optional: ASIO SDK for low-latency audio driver support

#### macOS
- Xcode 12 or newer (includes required command-line tools)
- macOS 10.15 Catalina or newer
- Core Audio framework (included with macOS)

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libasound2-dev
sudo apt-get install libjack-jackd2-dev
sudo apt-get install libfreetype6-dev
sudo apt-get install libx11-dev
sudo apt-get install libxrandr-dev
sudo apt-get install libxinerama-dev
sudo apt-get install libxcursor-dev
```

For other distributions, install equivalent development libraries for ALSA, JACK, FreeType, and X11.

## Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/GeorgeD88/AudioLoopStation
cd AudioLoopStation
```

### 2. Build with CLion (Recommended)

1. Open CLion
2. File → Open → Select the `AudioLoopStation` folder
3. CLion will detect `CMakeLists.txt` and configure the project automatically
4. Click **Build** (hammer icon) or press `Ctrl+F9` (Windows/Linux) / `Cmd+F9` (macOS)
5. Click **Run** (play icon) or press `Shift+F10` (Windows/Linux) / `Ctrl+R` (macOS)

### 3. Build from Command Line

#### Windows
TBD

#### Linux
TBD

#### macOS (Xcode)

1. Generate Xcode project (Apple Silicon):

```bash
cmake -S . -B build/xcode -G Xcode -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
```

2. Build:

- Option A — Xcode GUI
  - Open the project: `open build/xcode/AudioLoopStation.xcodeproj`
  - In Xcode, select the `AudioLoopStation_Standalone` scheme/target and Run.

- Option B — CMake CLI (use this if you hit Xcode signing issues)
  ```bash
  cmake --build build/xcode --config Debug --target AudioLoopStation_Standalone -- -destination 'platform=macOS,arch=arm64' CODE_SIGNING_ALLOWED=NO
  ```

3. Run the app:

```bash
open build/xcode/AudioLoopStation_artefacts/Debug/Standalone/AudioLoopStation.app
```

Notes:
- The `-destination ...` and `CODE_SIGNING_ALLOWED=NO` flags are Xcode‑specific (they are forwarded to `xcodebuild` by CMake). Other generators will ignore them.
- The first configure step will fetch JUCE via CPM (ensure you have network access)
 - If macOS refuses to launch the app due to security (Gatekeeper), right‑click the app and choose Open. If that doesn’t work, go to System Settings → Privacy & Security and click “Open Anyway”.
 

#### Run the Application
TBD

## Project Structure

TBD

## Current Development Status

**Progress Report #2 - November 2024**

### ✅ Completed
- Project structure and CMake build system
- JUCE application framework integration
- Basic main window and UI layout
- Audio device enumeration and selection
- Development environment setup documentation

### 🚧 In Progress
- Track recording functionality
- Audio I/O pipeline implementation
- UI components (track controls, transport buttons)
- Circular buffer for loop playback

### 📋 Planned (Next Sprint)
- Multi-track mixer with volume/pan controls
- Loop synchronization engine
- Mute/solo functionality
- Project save/load system

## Testing Your Build

After building successfully, verify the application works:

1. **Application launches** - Main window appears with no crashes
2. **Audio device selection** - Dropdown menu shows available audio devices
3. **UI renders correctly** - Simple square UI for now
4. **No console errors** - Check terminal/console output for errors

## Usage (Current Features)

### Audio Configuration
1. Launch the application
2. Settings → Audio/MIDI Settings
3. Select your audio input device (microphone/interface)
4. Select your audio output device (speakers/headphones)
5. Adjust buffer size (256 samples recommended for most systems)

### Basic Recording (Coming Soon)
Currently implementing: Click Record on Track 1 to begin recording your first loop.

## Development Workflow

### Setting Up Your Development Environment

1. Clone the repository
2. Open project in CLion
3. Create a feature branch: `git checkout -b yourname/dev`
4. Make changes and test locally
5. Commit with descriptive messages: `git commit -m "Add track recording functionality"`
6. Push to your fork: `git push origin feature/your-feature-name`
7. Open a Pull Request

### Code Style
- Follow existing code formatting
- Use descriptive variable and function names
- Comment complex logic
- Keep functions focused and concise

### Building for Development
Use Debug configuration for development with extra logging:

## Team

**CS 461 - Fall 2024**
- Vincewa Tran
- Maddox Nehls
- George Doujaiji
- Besher Al Maleh

## License

TBD

## Acknowledgments

- Built with [JUCE Framework](https://juce.com/) by ROLI Ltd.
- Inspired by classic hardware loop stations (Boss RC-505, Electro-Harmonix 95000)
- Oregon State University CS 461 Capstone Project


**Status**: Active Development | **Version**: 0.1.0-alpha | **Last Updated**: November 2024