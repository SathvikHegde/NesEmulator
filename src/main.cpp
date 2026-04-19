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
        
        std::string customRom = "";
        if (argc > 1) {
            for (int i = 1; i < argc; i++) {
                customRom += argv[i];
                if (i != argc - 1) customRom += " ";
            }
        }

        bool emulator_running = false;
        
        if (!customRom.empty()) {
            std::shared_ptr<Cartridge> cart = std::make_shared<Cartridge>(customRom);
            if (cart->ImageValid()) {
                nes.insertCartridge(cart);
                nes.reset();
                emulator_running = true;
            }
        }

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
        
        std::vector<std::string> palFiles;
        std::string palDir = "palettes";
        if (std::filesystem::exists(palDir) && std::filesystem::is_directory(palDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(palDir)) {
                if (entry.path().extension() == ".pal") {
                    palFiles.push_back(entry.path().filename().string());
                }
            }
        }
        int selectedPal = -1;

        std::vector<std::string> romFiles;
        std::string romDir = "roms";
        if (std::filesystem::exists(romDir) && std::filesystem::is_directory(romDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(romDir)) {
                if (entry.path().extension() == ".nes") {
                    romFiles.push_back(entry.path().filename().string());
                }
            }
        }

        // A, B, Select, Start, Up, Down, Left, Right
        int keybinds[8] = { GLFW_KEY_X, GLFW_KEY_Z, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_ENTER, GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D };
        const char* keybindNames[8] = { "A", "B", "Select", "Start", "Up", "Down", "Left", "Right" };
        int awaiting_key_bind_index = -1;

        const std::chrono::nanoseconds frame_duration(16639267);
        auto time_previous = std::chrono::high_resolution_clock::now();

        while (!renderer.shouldClose()) {
            glfwPollEvents();

            if (awaiting_key_bind_index != -1) {
                // Poll all standard GLFW keys
                for (int k = 32; k <= 348; k++) {
                    if (glfwGetKey(renderer.window, k) == GLFW_PRESS) {
                        keybinds[awaiting_key_bind_index] = k;
                        awaiting_key_bind_index = -1;
                        break;
                    }
                }
            }

            if (emulator_running && awaiting_key_bind_index == -1) {
                nes.controller[0] = 0x00;
                for (int i = 0; i < 8; i++) {
                    nes.controller[0] |= renderer.getKey(keybinds[i]) ? (0x80 >> i) : 0x00;
                }

                do {
                    nes.clock();
                } while (!nes.ppu.frame_complete);

                nes.ppu.frame_complete = false;
            }

            renderer.updateTexture(nes.ppu.screen);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin("Emulator Menu");

            if (ImGui::BeginTabBar("MenuTabs")) {
                if (ImGui::BeginTabItem("ROMs")) {
                    if (ImGui::Button("Refresh Discovered ROMs")) {
                        romFiles.clear();
                        if (std::filesystem::exists(romDir) && std::filesystem::is_directory(romDir)) {
                            for (const auto& entry : std::filesystem::directory_iterator(romDir)) {
                                if (entry.path().extension() == ".nes") {
                                    romFiles.push_back(entry.path().filename().string());
                                }
                            }
                        }
                    }
                    ImGui::Separator();
                    
                    if (ImGui::BeginListBox("##romsList", ImVec2(-FLT_MIN, 15 * ImGui::GetTextLineHeightWithSpacing()))) {
                        for (int i = 0; i < romFiles.size(); i++) {
                            if (ImGui::Selectable(romFiles[i].c_str())) {
                                std::shared_ptr<Cartridge> cart = std::make_shared<Cartridge>(romDir + "/" + romFiles[i]);
                                if (cart->ImageValid()) {
                                    nes.insertCartridge(cart);
                                    nes.reset();
                                    emulator_running = true;
                                }
                            }
                        }
                        ImGui::EndListBox();
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Video")) {
                    ImGui::Text("Color Palette:");
                    if (ImGui::BeginCombo("##palette", selectedPal >= 0 ? palFiles[selectedPal].c_str() : "Default (FBX Smooth)")) {
                        for (int i = 0; i < palFiles.size(); i++) {
                            bool is_selected = (selectedPal == i);
                            if (ImGui::Selectable(palFiles[i].c_str(), is_selected)) {
                                selectedPal = i;
                                nes.ppu.loadCustomPalette(palDir + "/" + palFiles[i]);
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::Text("Click a button then press any key to bind:");
                    ImGui::Separator();
                    for (int i = 0; i < 8; i++) {
                        ImGui::Text("%s", keybindNames[i]);
                        ImGui::SameLine(100);
                        
                        std::string btnLabel = awaiting_key_bind_index == i ? "PRESS A KEY" : std::to_string(keybinds[i]);
                        btnLabel += "##key" + std::to_string(i);
                        
                        if (ImGui::Button(btnLabel.c_str(), ImVec2(150, 0))) {
                            awaiting_key_bind_index = i;
                        }
                    }
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }

            ImGui::End();

            ImGui::Render();
            renderer.drawFrame();

            // Software Frame Limiter
            auto time_current = std::chrono::high_resolution_clock::now();
            auto time_elapsed = time_current - time_previous;
            
            // I have stared into the abyss, and the abyss stared back.
            // It looked exactly like this spin-lock loop.
            if (time_elapsed < frame_duration) {
                // DO NOT TOUCH THIS. I don't know why std::this_thread::sleep_for doesn't work, 
                // but if you remove this spinlock, the audio catches fire and Mario runs at Mach 5.
                // Just leave it. It's load-bearing now.
                std::chrono::nanoseconds time_remaining = frame_duration - time_elapsed;
                while (std::chrono::high_resolution_clock::now() - time_previous < frame_duration) {
                    // Spin. And ponder your existence.
                }
            }
            time_previous = std::chrono::high_resolution_clock::now();
        }

        ma_device_uninit(&audio_device);
        std::cout << "Emulator shutting down." << std::endl;
    } catch (const std::exception& e) {
        // Shhh. Go to sleep.
        std::cerr << "\\nFATAL C++ EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
