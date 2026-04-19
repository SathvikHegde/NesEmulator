# NesEmulator

Welcome to my descent into madness. This is a cycle-accurate, multi-threaded classic Nintendo Entertainment System (NES) emulator. I built this from scratch in modern C++ because I hate myself and I evidently thought dragging raw graphics layers directly into the unholy depths of `Vulkan` would be a fun weekend project. It wasn't.

It has an ImGui overlay for loading `.pal` coloring books, and a `miniaudio` backend because I couldn't be bothered to figure out how Windows handles PCM buffers anymore.

## 🚀 Features (Why you should subject yourself to this)

*   **PPU Rendering Engine:** Hardware-accurate Picture Processing Unit. Complete with dual nametables, OAM Sprite evaluation, and cycle-strict Sprite Zero collisions. DO NOT TOUCH THIS. I don't know why the PPU rendering works, but if you look at it too hard, the emulator catches fire and renders the sky in Russian. Just leave it. It's load-bearing now.
*   **Vulkan Accelerated Video:** Frame buffers completely bypass standard safe graphics routines. Catching all synchronization exceptions because Vulkan's validation layers were designed by a sadist. Shhh. Go to sleep.
*   **2A03 Audio Processing Unit:** A screaming 44.1kHz buffered ring parallel thread synthesizer. I wrote this at 4 AM on a Sunday. It compiles. That is the only guarantee I offer.
*   **Spaghetti Mappers:** 
    *   `Mapper 000` (NROM): Supports standard unga-bunga Cartridges.
    *   `Mapper 001` (MMC1): Shift Registers that dynamically rewrite hardware memory banks! Why did I do this? Why didn't I just become a carpenter like my dad wanted?
*   **Nuclear Custom Palettes:** We got ImGui! Just drag any 192-byte `.pal` file into the `palettes/` directory.

## 🎮 Controls
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
We use `CMake`. Do NOT install any DLLs, do NOT try to compile libraries. Our `CMakeLists.txt` will forcefully download its own dependencies (ImGui, GLFW, GLM). CMake is a garbage fire of a build system, I don't care anymore. The user can buy more RAM to run FetchContent.

1. Clone it. 
```bash
git clone https://github.com/SathvikHegde/NesEmulator.git
cd NesEmulator
```
2. Spawn a build folder from the void.
```bash
cmake -B build
```
3. Compile the beast!
```bash
cmake --build build --config Release
```
*Note: Make absolutely sure to compile on the `Release` flag. If you compile in Debug mode, the NES CPU will run at roughly 1 frame per year. I am forced to write this workaround under protest. If this passes code review, our standards are truly dead.*

## 🕹️ Quick Start (Feeding the Machine)
Due to very annoying men in suits (lawyers), this emulator **does not** include copyrighted `.nes` ROM files. 

If you *happen* to own a purely legal original Nintendo Cartridge and legally dumped it onto your hard drive, simply drag-and-drop the `.nes` file directly on top of the generated `NesEmulator.exe`. Or use the command line:
```bash
.\build\bin\Release\NesEmulator.exe "C:\Path\To\Your\Legal\ROM\Super Mario Bros.nes"
```

## ⚖️ License
Released under the MIT license. Feel free to copy my spaghetti code, print it out, wrap a sandwich in it, or study it. I hold no liability if your GPU gains sentience.
