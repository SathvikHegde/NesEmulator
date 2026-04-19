# NesEmulator: The Most Radioactive C++20 Vulkan Experience™

Welcome to my descent into madness. This is a cycle-accurate, occasionally homicidal, multi-threaded classic Nintendo Entertainment System (NES) emulator. I built this from scratch in modern C++ because I hate myself and I love dragging raw graphics layers directly into the unholy depths of `Vulkan`. 

It has an ImGui overlay for loading `.pal` coloring books, and a `miniaudio` backend that mathematically generates sound waves so accurately it will likely alert neighborhood dogs to your location.

## 🚀 Features (Why you should subject yourself to this)

*   **PPU Rendering Engine:** Hardware-accurate Picture Processing Unit. Complete with dual nametables, OAM Sprite evaluation, and cycle-strict Sprite Zero collisions. Honestly, the PPU is held together by duct tape and prayers, but it pumps out identical Nestopia colors so nobody complain. 
*   **Vulkan Accelerated Video:** Frame buffers completely bypass standard safe graphics routines. We inject raw binary directly into your monitor's swapchain because living dangerously is the only way to live.
*   **2A03 Audio Processing Unit:** A screaming 44.1kHz buffered ring parallel thread synthesizer.
*   **Spaghetti Mappers:** 
    *   `Mapper 000` (NROM): Supports standard unga-bunga Cartridges *(e.g., Donkey Kong, Balloon Fight)*
    *   `Mapper 001` (MMC1): Shift Registers that dynamically rewrite hardware memory banks! *(e.g., Zelda)*
*   **Nuclear Custom Palettes:** We got ImGui! Just drag any 192-byte `.pal` file into the `palettes/` directory and hot-swap your childhood colors while playing. Turn Mario into a smurf. We don't judge.

## 🎮 Controls (Keyboard mapped to Controller 1. Get a gamepad, nerd.)
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
We use `CMake`. Do NOT install any DLLs, do NOT try to compile libraries. Our `CMakeLists.txt` will forcefully download its own dependencies (ImGui, GLFW, GLM) because we have trust issues.

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
*Note: Make absolutely sure to compile on the `Release` flag. If you compile in Debug mode, the NES CPU will run at roughly 1 frame per year.*

## 🕹️ Quick Start (Feeding the Machine)
Due to very annoying men in suits (lawyers), this emulator **does not** include copyrighted `.nes` ROM files. 

If you *happen* to own a purely legal original Nintendo Cartridge and legally dumped it onto your hard drive, simply drag-and-drop the `.nes` file directly on top of the generated `NesEmulator.exe`. Or use the command line like a hacker from a 90s movie:
```bash
.\build\bin\Release\NesEmulator.exe "C:\Path\To\Your\Legal\ROM\Super Mario Bros.nes"
```

## ⚖️ License
Released under the MIT license. Feel free to copy my spaghetti code, print it out, wrap a sandwich in it, or study it. I hold no liability if your GPU gains sentience.
