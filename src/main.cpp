#include <iostream>
#include <fstream>
#include "Bus.h"
#include "CPU.h"
#include "Renderer.h"
#include "Cartridge.h"
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <filesystem>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// Hardware Audio Callback
void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    Bus* nes = (Bus*)pDevice->pUserData;
    float* pOutputF32 = (float*)pOutput;
    
    std::lock_guard<std::mutex> lock(nes->audio_mutex);
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        if (!nes->audio_samples.empty()) {
            double sample = nes->audio_samples.front();
            nes->audio_samples.pop();
            pOutputF32[i * 2 + 0] = (float)sample; // Stereo Left
            pOutputF32[i * 2 + 1] = (float)sample; // Stereo Right
        } else {
            pOutputF32[i * 2 + 0] = 0.0f;
            pOutputF32[i * 2 + 1] = 0.0f;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "Initializing NES Emulator..." << std::endl;

        Bus nes;
        
        std::string romPath = "nestest.nes";
        if (argc > 1) {
            romPath = "";
            for (int i = 1; i < argc; i++) {
                romPath += argv[i];
                if (i != argc - 1) romPath += " ";
            }
        }

        std::shared_ptr<Cartridge> cart = std::make_shared<Cartridge>(romPath);
        if (!cart->ImageValid()) {
            std::cerr << "WARNING: Could not load '" << romPath << "'. Check directory or format!" << std::endl;
            system("pause");
            return 1;
        } else {
            nes.insertCartridge(cart);
        }

        nes.reset();

        std::cout << "Booting Vulkan Renderer..." << std::endl;
        Renderer renderer;

        std::cout << "Booting Sound Engine..." << std::endl;
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format   = ma_format_f32;
        config.playback.channels = 2; // Stereo output
        config.sampleRate        = 44100;
        config.dataCallback      = audio_callback;
        config.pUserData         = &nes;

        ma_device audio_device;
        if (ma_device_init(NULL, &config, &audio_device) != MA_SUCCESS) {
            std::cerr << "WARNING: Failed to capture audio endpoint! Audio will be muted." << std::endl;
        } else {
            ma_device_start(&audio_device);
        }

        std::cout << "Starting Emulator Loop!" << std::endl;
        
        // Scan for .pal files in the current directory
        std::vector<std::string> palFiles;
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.path().extension() == ".pal") {
                palFiles.push_back(entry.path().filename().string());
            }
        }
        int selectedPal = -1;

        // NES NTSC frame duration = ~16.639 milliseconds
        const std::chrono::nanoseconds frame_duration(16639267);
        auto time_previous = std::chrono::high_resolution_clock::now();

        while (!renderer.shouldClose()) {
            glfwPollEvents();

            // Handle Input Mapping
            // A, B, Select, Start, Up, Down, Left, Right
            nes.controller[0] = 0x00;
            nes.controller[0] |= renderer.getKey(GLFW_KEY_X) ? 0x80 : 0x00; // A
            nes.controller[0] |= renderer.getKey(GLFW_KEY_Z) ? 0x40 : 0x00; // B
            nes.controller[0] |= renderer.getKey(GLFW_KEY_RIGHT_SHIFT) ? 0x20 : 0x00; // Select
            nes.controller[0] |= renderer.getKey(GLFW_KEY_ENTER) ? 0x10 : 0x00; // Start
            nes.controller[0] |= renderer.getKey(GLFW_KEY_W) ? 0x08 : 0x00; // Up
            nes.controller[0] |= renderer.getKey(GLFW_KEY_S) ? 0x04 : 0x00; // Down
            nes.controller[0] |= renderer.getKey(GLFW_KEY_A) ? 0x02 : 0x00; // Left
            nes.controller[0] |= renderer.getKey(GLFW_KEY_D) ? 0x01 : 0x00; // Right

            do {
                nes.clock();
            } while (!nes.ppu.frame_complete);

            nes.ppu.frame_complete = false;

            renderer.updateTexture(nes.ppu.screen);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Emulator Settings");
            ImGui::Text("Color Palette:");
            if (ImGui::BeginCombo("##palette", selectedPal >= 0 ? palFiles[selectedPal].c_str() : "Default (FBX Smooth)")) {
                for (int i = 0; i < palFiles.size(); i++) {
                    bool is_selected = (selectedPal == i);
                    if (ImGui::Selectable(palFiles[i].c_str(), is_selected)) {
                        selectedPal = i;
                        nes.ppu.loadCustomPalette(palFiles[i]);
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::End();

            ImGui::Render();
            renderer.drawFrame();

            // Software Frame Limiter
            auto time_current = std::chrono::high_resolution_clock::now();
            auto time_elapsed = time_current - time_previous;
            
            if (time_elapsed < frame_duration) {
                // If we finished the frame early, wait out the remaining microseconds
                std::chrono::nanoseconds time_remaining = frame_duration - time_elapsed;
                // std::this_thread::sleep_for is sometimes wildly inaccurate on Windows. 
                // A spin-lock / micro-sleep hybrid keeps exact timing:
                while (std::chrono::high_resolution_clock::now() - time_previous < frame_duration) {
                    // Spin
                }
            }
            time_previous = std::chrono::high_resolution_clock::now();
        }

        ma_device_uninit(&audio_device);
        std::cout << "Emulator shutting down." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\\nFATAL C++ EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
