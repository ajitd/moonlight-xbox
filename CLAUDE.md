# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**moonlight-xbox** is a UWP (Universal Windows Platform) application that ports Moonlight Stream to Xbox One and Xbox Series X|S consoles. It enables game streaming from GeForce Experience or Sunshine host applications using H.264, H.265/HEVC, and AV1 codecs.

## Build Commands

### Initial Setup
1. **Clone with submodules**: `git submodule update --init --recursive`
2. **Bootstrap vcpkg**: `vcpkg\bootstrap-vcpkg.bat`
3. **Install dependencies**: `.\vcpkg\vcpkg.exe install --triplet x64-uwp`

### Build Process
1. **Generate third-party projects**: `generate-thirdparty-projects.bat`
2. **Build libgamestream**: Navigate to `libgamestream` directory and run the CMake command from `generate-thirdparty-projects.bat`
3. **Open solution**: `moonlight-xbox-dx.sln` in Visual Studio 2022

### Key Build Scripts
- `generate-thirdparty-projects.bat`: Generates CMake projects for moonlight-common-c and libgamestream with UWP targeting
- `Assets/Shader/build_hlsl.bat`: Compiles HLSL shaders for video rendering

## Architecture Overview

### Core Components

**Application Layer**
- `App.xaml.cpp`: Main UWP application entry point and lifecycle management
- `MoonlightWelcome.xaml.*`: Primary welcome/launcher UI
- `Pages/`: Contains all XAML pages for different app screens

**State Management**
- `State/MoonlightClient.*`: Core client logic interfacing with Limelight protocol
- `State/MoonlightHost.*`: Host representation with streaming configuration (bitrate, resolution, codecs)
- `State/ApplicationState.*`: Global app state management
- `State/StreamConfiguration.*`: Streaming parameter configuration

**Streaming Engine**
- `Streaming/FFmpegDecoder.*`: Hardware-accelerated video decoding using FFmpeg with D3D11 integration
- `Streaming/VideoRenderer.*`: DirectX 11 video rendering with HDR support
- `Streaming/AudioPlayer.*`: Audio playback handling
- `Streaming/moonlight_xbox_dxMain.*`: Core DirectX rendering loop

**Input System**
- `KeyboardControl.xaml.*`: On-screen keyboard implementation
- `Keyboard/`: Extensive keyboard layout definitions for international support
- Input handling integrated into MoonlightClient for gamepad, keyboard, and virtual mouse

**Third-Party Libraries**
- `libgamestream/`: GameStream protocol implementation (C library)
- `third_party/moonlight-common-c/`: Core Moonlight streaming protocol
- `third_party/DirectXTK/`: DirectX Toolkit for rendering utilities

### Key Technical Details

**Video Decoding**: Uses FFmpeg with D3D11VA hardware acceleration, supporting H.264, H.265 (Xbox Series only), and AV1 (Xbox Series only)

**Audio**: Integrated audio streaming with configurable output (stereo, surround)

**HDR Support**: Full HDR10 pipeline with appropriate colorspace handling

**Input Processing**: Gamepad rumble, keyboard input (both hardware and virtual), and mouse emulation via gamepad

**Host Discovery**: mDNS-based host discovery and manual IP configuration

## Configuration Files

- `vcpkg.json`: Package dependencies (FFmpeg, OpenSSL, cURL, etc.)
- `Package.appxmanifest`: UWP app manifest with capabilities and declarations
- `moonlight-xbox-dx.vcxproj`: Main Visual Studio project file

## Dependencies

Primary external dependencies managed through vcpkg:
- **FFmpeg**: Video/audio codec support with hardware acceleration
- **OpenSSL**: Cryptographic operations for secure streaming
- **cURL**: HTTP client for GameStream protocol communication
- **nlohmann-json**: JSON parsing for host communication
- **Opus**: Audio codec support

## Development Notes

**Platform Constraints**: UWP limitations prevent hardware mouse support - mouse functionality is emulated through gamepad input.

**Xbox Variants**: Different codec support based on hardware (H.265/AV1 only on Series S/X)

**Build Requirements**: Windows 10, Visual Studio 2022, and vcpkg for dependency management.

**Streaming Configuration**: Host-specific settings stored per connection, including bitrate limits (configurable up to 500 Mbps), resolution, FPS, and codec selection.