# NesEmulator (C++20 & Vulkan)

A cycle-accurate, multi-threaded classic Nintendo Entertainment System (NES) emulator engineered from scratch in modern C++. Built purely for accuracy, speed, and architectural clarity, this emulator leverages raw `Vulkan` hardware acceleration for zero-latency screen-rendering, and a thread-safe `miniaudio` backend to reproduce perfect 8-bit sound arrays locally.

## 🚀 Features

*   **PPU Rendering Engine:** Hardware-accurate Picture Processing Unit mapping dual nametables, OAM Sprite evaluation, Palette indexing, and cycle-strict Sprite Zero collision logic. 
*   **Vulkan Accelerated Video:** Frame buffers bypass standard graphical libraries directly targeting OS window Swapchains natively through raw Vulkan.
*   **2A03 Audio Processing Unit:** Natively mathematical oscillators synthesizing Pulse, Triangle, and local LFSR Noise waves on an asynchronous 44.1kHz buffered ring parallel thread.  
*   **Dynamic Cartridge Mapping (Mappers):** 
    *   `Mapper 000` (NROM): Supports standard 16KB/32KB Cartridges *(e.g., Donkey Kong, Balloon Fight)*
    *   `Mapper 001` (MMC1): Emulates native Shift Registers dynamically rewriting hardware memory banks horizontally *(e.g., Super Mario Bros. Multicarts, Zelda)*
*   **No Dependency Hell:** Relies entirely on CMake's `FetchContent`. Zero massive dynamic UI dependencies like `SDL2`. It automatically downloads its own lightweight `glfw` and `glm` vectors!

## 🎮 Controls (Keyboard mapped exclusively to Controller 1)
| NES Button | Keyboard Key |
| :--- | :--- |
| **D-Pad Right** | `D` |
| **D-Pad Left** | `A` |
| **D-Pad Up** | `W` |
| **D-Pad Down** | `S` |
| **A Button** | `X` |
| **B Button** | `Z` |
| **Start** | `Enter` |
| **Select** | `Right Shift` |

## 🛠️ Build Instructions
This repository utilizes standard cross-platform `CMake`. You do not need to install complicated DLLs locally; `CMake` fetches everything autonomously!

1. Clone the repository locally.
```bash
git clone https://github.com/your-username/NesEmulator.git
cd NesEmulator
```
2. Configure CMake internally (creates a build directory).
```bash
cmake -B build
```
3. Build the Release Executable!
```bash
cmake --build build --config Release
```
*Note: Make absolutely sure to compile on the `Release` flag for correct internal 60FPS lock execution, otherwise the physical NES CPUs run far too slow in IDE Debug modes!*

## 🕹️ Quick Start (Running a Game)
Due to licensing and IP Laws, this emulator **does not** include copyrighted commercial `.nes` ROM files automatically. 

However, running your own personal backups is extremely fast. Once you have built the executable, you can simply **Drag-and-Drop** your `.nes` file directly on top of the generated `NesEmulator.exe` shortcut internally through Windows!

Alternatively, run it dynamically via Command Line:
```bash
# Executing natively
.\build\bin\Release\NesEmulator.exe "C:\Path\To\Your\ROM\Super Mario Bros.nes"
```

## ⚖️ License
Released under the standard MIT open-source license. Feel absolutely free to clone, edit, port to WebAssembly, or study the C++ hardware mapping layers for learning.
