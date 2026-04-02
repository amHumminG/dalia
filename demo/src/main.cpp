// Raylib / ImGui
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"

#include <chrono>
#include <thread>
#include <iostream>

using namespace dalia;

// --- Helper Structs for the UI State ---
struct LoadedAsset {
    std::string path;
    SoundHandle handle;
};

struct PlaybackInstance {
    std::string name;
    PlaybackHandle handle;
    std::string targetBus = "Master"; // Default route
    bool isLooping = false;
    float volumeDb = 0.0f;
    float pan = 0.0f;
};

struct LoadedEffect {
    std::string name;
    EffectHandle handle;
    std::string attachedBus = "None"; // Tracking state
    int attachedSlot = -1;            // Tracking state
};

// NEW: Struct to track Bus topology
struct BusNode {
    std::string name;
    std::string parentBus;
    float volumeDb = 0.0f; // Track volume state (0.0f is unity gain)
};

void TestInterface() {
    // --- 1. Initialize Raylib and ImGui ---
    InitWindow(1400, 800, "Dalia Engine - Audio Testbed"); // Widened for 4 columns
    SetTargetFPS(60);
    rlImGuiSetup(true);

    // --- 2. Initialize Dalia Engine ---
    Engine engine;
    EngineConfig config;
    config.logLevel = LogLevel::Debug;
    engine.Init(config);

    // --- 3. UI State Variables ---
    char assetPathInput[256] = "assets/unethical stereo 48kHz.ogg";

    std::vector<LoadedAsset> loadedAssets;
    std::vector<PlaybackInstance> playbacks;
    std::vector<LoadedEffect> loadedEffects;

    // Default buses that should always exist in a mix topology
    std::vector<BusNode> createdBuses = { {"Master", "None", 0.0f} };

    int selectedAssetIdx = -1;
    int selectedPlaybackIdx = -1;
    int selectedEffectIdx = -1;
    int selectedBusIdx = -1;

    // Effect Parameter State
    int selectedBiquadType = 0;
    const char* biquadTypeNames[] = { "LowPass", "HighPass", "BandPass" };
    float effectFreq = 20000.0f;
    float effectRes = 0.707f;

    // Routing State
    char targetBusInput[64] = "Master";
    int targetSlotIndex = 0;

    char newBusNameInput[64] = "SFX";
    char parentBusInput[64] = "Master";
    char playbackTargetBus[64] = "Master";

    Result lastResult = Result::Ok;

    // --- 4. Main Game Loop ---
    while (!WindowShouldClose()) {
        engine.Update();

        BeginDrawing();
        ClearBackground(DARKGRAY);
        rlImGuiBegin();

        ImGui::SetNextWindowSize(ImVec2(1360, 760), ImGuiCond_FirstUseEver);
        ImGui::Begin("Engine Inspector", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Last Engine API Result: %d", static_cast<int>(lastResult));
        ImGui::Separator();

        // --- SPLIT INTO FOUR COLUMNS ---
        ImGui::Columns(4, "MainColumns");

        // ==========================================
        // COLUMN 1: ASSET MANAGEMENT
        // ==========================================
        ImGui::TextDisabled("ASSET REGISTRY");
        ImGui::Spacing();

        ImGui::InputText("Filepath", assetPathInput, 256);
        ImGui::Spacing();

        if (ImGui::Button("Load to RAM", ImVec2(100, 30))) {
            SoundHandle h;
            lastResult = engine.LoadSoundAsync(h, SoundType::Resident, assetPathInput);
            if (lastResult == Result::Ok) loadedAssets.push_back({assetPathInput, h});
        }
        ImGui::SameLine();
        if (ImGui::Button("Load to Disk", ImVec2(100, 30))) {
            SoundHandle h;
            lastResult = engine.LoadSoundAsync(h, SoundType::Stream, assetPathInput);
            if (lastResult == Result::Ok) loadedAssets.push_back({assetPathInput, h});
        }

        ImGui::Spacing();
        ImGui::BeginChild("AssetList", ImVec2(0, 200), true);
        for (int i = 0; i < loadedAssets.size(); i++) {
            SoundHandle h = loadedAssets[i].handle;
            std::string typeTag = (h.GetType() == SoundType::Resident) ? "[RAM] " : "[DSK] ";
            char label[256];
            snprintf(label, sizeof(label), "%s%s (0x%08X)", typeTag.c_str(), loadedAssets[i].path.c_str(), h.GetIndex());

            if (ImGui::Selectable(label, selectedAssetIdx == i)) selectedAssetIdx = i;
        }
        ImGui::EndChild();

        if (ImGui::Button("Unload") && selectedAssetIdx >= 0 && selectedAssetIdx < loadedAssets.size()) {
            lastResult = engine.UnloadSound(loadedAssets[selectedAssetIdx].handle);
            loadedAssets.erase(loadedAssets.begin() + selectedAssetIdx);
            selectedAssetIdx = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Playback") && selectedAssetIdx >= 0 && selectedAssetIdx < loadedAssets.size()) {
            PlaybackHandle pHandle;
            lastResult = engine.CreatePlayback(pHandle, loadedAssets[selectedAssetIdx].handle);
            if (lastResult == Result::Ok) {
                playbacks.push_back({loadedAssets[selectedAssetIdx].path, pHandle});
            }
        }

        ImGui::NextColumn();

        // ==========================================
        // COLUMN 2: PLAYBACK VOICES
        // ==========================================
        ImGui::TextDisabled("VOICE MIXER");
        ImGui::Spacing();

        ImGui::Text("Active Playback Instances");
        ImGui::BeginChild("PlaybackList", ImVec2(0, 200), true);
        for (int i = 0; i < playbacks.size(); i++) {
            char label[256];
            snprintf(label, sizeof(label), "%s (0x%016llx)", playbacks[i].name.c_str(), playbacks[i].handle.GetUUID());
            if (ImGui::Selectable(label, selectedPlaybackIdx == i)) selectedPlaybackIdx = i;
        }
        ImGui::EndChild();

        if (selectedPlaybackIdx >= 0 && selectedPlaybackIdx < playbacks.size()) {
            PlaybackHandle currentHandle = playbacks[selectedPlaybackIdx].handle;

            // Transport
            if (ImGui::Button("Play", ImVec2(60, 30))) lastResult = engine.Play(currentHandle);
            ImGui::SameLine();
            if (ImGui::Button("Pause", ImVec2(60, 30))) lastResult = engine.Pause(currentHandle);
            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(60, 30))) lastResult = engine.Stop(currentHandle);

            ImGui::Spacing();
            bool currentLoopState = playbacks[selectedPlaybackIdx].isLooping;
            if (ImGui::Checkbox("Loop Playback", &currentLoopState)) {
                lastResult = engine.SetPlaybackLooping(currentHandle, currentLoopState);
                if (lastResult == Result::Ok) {
                    // Only update the UI state if the engine accepted the command
                    playbacks[selectedPlaybackIdx].isLooping = currentLoopState;
                }
            }

            ImGui::Separator();

            ImGui::TextDisabled("Playback Parameters");

            float currentVol = playbacks[selectedPlaybackIdx].volumeDb;
            if (ImGui::SliderFloat("Volume (dB)##PB", &currentVol, -60.0f, 12.0f, "%.1f dB")) {
                lastResult = engine.SetPlaybackVolumeDb(currentHandle, currentVol);
                if (lastResult == Result::Ok) {
                    playbacks[selectedPlaybackIdx].volumeDb = currentVol;
                }
            }

            float currentPan = playbacks[selectedPlaybackIdx].pan;
            if (ImGui::SliderFloat("Pan##PB", &currentPan, -1.0f, 1.0f, "%.2f")) {
                lastResult = engine.SetPlaybackStereoPan(currentHandle, currentPan);
                if (lastResult == Result::Ok) {
                    playbacks[selectedPlaybackIdx].pan = currentPan;
                }
            }

            // Playback Routing
            ImGui::TextDisabled("Playback Routing");

            // SHOW CURRENT ROUTE
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Current Bus: %s", playbacks[selectedPlaybackIdx].targetBus.c_str());

            ImGui::InputText("Target Bus##PB", playbackTargetBus, 64);
            if (ImGui::Button("Route Voice to Bus", ImVec2(140, 30))) {
                lastResult = engine.RoutePlayback(currentHandle, playbackTargetBus);
                if (lastResult == Result::Ok) {
                    playbacks[selectedPlaybackIdx].targetBus = playbackTargetBus; // Update UI State
                }
            }
        } else {
            ImGui::TextDisabled("Select a playback instance.");
        }

        ImGui::NextColumn();

// ==========================================
        // COLUMN 3: BUS ARCHITECTURE
        // ==========================================
        ImGui::TextDisabled("BUS TOPOLOGY");
        ImGui::Spacing();

        ImGui::InputText("Bus Name##NewBus", newBusNameInput, 64);
        if (ImGui::Button("Create Bus", ImVec2(120, 30))) {
            lastResult = engine.CreateBus(newBusNameInput);
            if (lastResult == Result::Ok) {
                // Initialize new buses with 0.0 dB (unity gain)
                createdBuses.push_back({newBusNameInput, "Master", 0.0f});
            }
        }

        ImGui::Spacing();
        ImGui::Text("Allocated Buses");
        ImGui::BeginChild("BusList", ImVec2(0, 150), true);
        for (int i = 0; i < createdBuses.size(); i++) {
            if (ImGui::Selectable(createdBuses[i].name.c_str(), selectedBusIdx == i)) {
                selectedBusIdx = i;
            }
        }
        ImGui::EndChild();

        if (selectedBusIdx >= 0 && selectedBusIdx < createdBuses.size()) {
            ImGui::TextDisabled("Bus Routing");

            // SHOW CURRENT PARENT
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Current Parent: %s", createdBuses[selectedBusIdx].parentBus.c_str());

            ImGui::InputText("Parent Bus", parentBusInput, 64);
            if (ImGui::Button("Set Parent Bus", ImVec2(120, 30))) {
                lastResult = engine.RouteBus(createdBuses[selectedBusIdx].name.c_str(), parentBusInput);
                if (lastResult == Result::Ok) {
                    createdBuses[selectedBusIdx].parentBus = parentBusInput;
                }
            }

            ImGui::Separator();

            // --- NEW: BUS PARAMETERS ---
            ImGui::TextDisabled("Bus Parameters");

            // Standard audio fader range: -60dB (basically silence) to +12dB (boost)
            if (ImGui::SliderFloat("Volume (dB)", &createdBuses[selectedBusIdx].volumeDb, -60.0f, 12.0f, "%.1f dB")) {
                // NOTE: Adjust to your specific SetBusVolume API signature
                engine.SetBusVolumeDb(createdBuses[selectedBusIdx].name.c_str(), createdBuses[selectedBusIdx].volumeDb);
            }

            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Destroy Bus", ImVec2(120, 30))) {
                lastResult = engine.DestroyBus(createdBuses[selectedBusIdx].name.c_str());
                if (lastResult == Result::Ok) {
                    createdBuses.erase(createdBuses.begin() + selectedBusIdx);
                    selectedBusIdx = -1;
                }
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Select a bus to manage.");
        }

        ImGui::NextColumn();

        // ==========================================
        // COLUMN 4: EFFECT RACK
        // ==========================================
        ImGui::TextDisabled("EFFECT RACK");
        ImGui::Spacing();

        ImGui::TextDisabled("New Biquad Filter");
        ImGui::Combo("Type", &selectedBiquadType, biquadTypeNames, IM_ARRAYSIZE(biquadTypeNames));

        if (ImGui::Button("Allocate Filter", ImVec2(150, 30))) {
            EffectHandle h;
            BiquadConfig biquadConfig;
            BiquadFilterType type = static_cast<BiquadFilterType>(selectedBiquadType);

            lastResult = engine.CreateBiquadFilter(h, type, biquadConfig);
            if (lastResult == Result::Ok) {
                std::string labelName = std::string(biquadTypeNames[selectedBiquadType]) + " Filter";
                // Add the new effect to the UI list with unattached default state
                loadedEffects.push_back({labelName, h, "None", -1});
            }
        }

        ImGui::Spacing();
        ImGui::Text("Allocated Effects (Memory)");
        ImGui::BeginChild("EffectList", ImVec2(0, 120), true);
        for (int i = 0; i < loadedEffects.size(); i++) {
            char label[256];
            snprintf(label, sizeof(label), "%s (0x%016llx)", loadedEffects[i].name.c_str(), loadedEffects[i].handle.GetUUID());
            if (ImGui::Selectable(label, selectedEffectIdx == i)) selectedEffectIdx = i;
        }
        ImGui::EndChild();

        if (selectedEffectIdx >= 0 && selectedEffectIdx < loadedEffects.size()) {
            EffectHandle currentEffect = loadedEffects[selectedEffectIdx].handle;

            ImGui::TextDisabled("DSP Parameters");
            bool paramsChanged = false;
            if (ImGui::SliderFloat("Frequency", &effectFreq, 20.0f, 20000.0f, "%.1f Hz", ImGuiSliderFlags_Logarithmic)) paramsChanged = true;
            if (ImGui::SliderFloat("Resonance", &effectRes, 0.1f, 10.0f, "%.3f")) paramsChanged = true;

            if (paramsChanged) {
                BiquadConfig biquadConfig = {effectFreq, effectRes};
                engine.SetBiquadParams(currentEffect, biquadConfig);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Bus Routing");

            // SHOW CURRENT ATTACHMENT
            if (loadedEffects[selectedEffectIdx].attachedSlot != -1) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Attached to: %s (Slot %d)",
                    loadedEffects[selectedEffectIdx].attachedBus.c_str(),
                    loadedEffects[selectedEffectIdx].attachedSlot);
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Currently Unattached");
            }

            ImGui::InputText("Target Bus##Fx", targetBusInput, 64);
            ImGui::SliderInt("Slot", &targetSlotIndex, 0, 3);

            if (ImGui::Button("Attach", ImVec2(60, 30))) {
                lastResult = engine.AttachEffect(currentEffect, targetBusInput, targetSlotIndex);
                if (lastResult == Result::Ok) {
                    loadedEffects[selectedEffectIdx].attachedBus = targetBusInput;
                    loadedEffects[selectedEffectIdx].attachedSlot = targetSlotIndex;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Detach", ImVec2(60, 30))) {
                // We detach from the currently input text field
                lastResult = engine.DetachEffect(currentEffect);
                if (lastResult == Result::Ok) {
                    loadedEffects[selectedEffectIdx].attachedBus = "None";
                    loadedEffects[selectedEffectIdx].attachedSlot = -1;
                }
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Destroy Effect", ImVec2(150, 30))) {
                lastResult = engine.DestroyEffect(currentEffect);
                // Note: We don't need to check Result::Ok here if we assume destruction always succeeds or cleans up the handle anyway.
                loadedEffects.erase(loadedEffects.begin() + selectedEffectIdx);
                selectedEffectIdx = -1;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Select an effect.");
        }

        ImGui::Columns(1);
        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
    }

    engine.Shutdown();
    rlImGuiShutdown();
    CloseWindow();
}

void SpidermanFinishedLoading(uint32_t requestId, Result result) {
    if (result == Result::Ok) {
        std::cout << "SPIDERMAN LOADED" << std::endl;
    }
    else {
        std::cout << "SPIDERMAN LOAD FAILED" << std::endl;
    }
}

void SpidermanFinishedPlaying(PlaybackHandle handle, PlaybackExitCondition exitCondition) {
    if (exitCondition == PlaybackExitCondition::NaturalEnd) {
        std::cout << "SPIDERMAN FINISHED NATURALLY" << std::endl;
    }
    else if (exitCondition == PlaybackExitCondition::ExplicitStop) {
        std::cout << "SPIDERMAN STOPPED EXPLICITLY" << std::endl;
    }
    else if (exitCondition == PlaybackExitCondition::Evicted) {
        std::cout << "SPIDERMAN EVICTED (lol)" << std::endl;
    }
    else if (exitCondition == PlaybackExitCondition::Error) {
        std::cout << "SPIDERMAN STOPPED BY ERROR" << std::endl;
    }
}

int main() {

    Engine engine;
    EngineConfig config;
    config.logLevel = LogLevel::Debug;
    engine.Init(config);


    // SFX
    // engine.CreateBus("SFX", "Master");
    // engine.SetBusVolumeDb("SFX", -10.0f);
    //
    // SoundHandle spidermanSound;
    // engine.LoadSoundAsync(spidermanSound, SoundType::Resident, "assets/spiderman.ogg", SpidermanFinishedLoading);
    //
    // PlaybackHandle spidermanPlayback;
    // engine.CreatePlayback(spidermanPlayback, spidermanSound, SpidermanFinishedPlaying);
    // engine.RoutePlayback(spidermanPlayback, "Music");
    //
    // engine.Play(spidermanPlayback);

    // // MUSIC
    // engine.CreateBus("Music", "Master");
    // engine.SetBusVolumeDb("Music", -20.0f);
    //
    // EffectHandle biquadFilter;
    // BiquadConfig biquadConfig;
    // biquadConfig.frequency = 400.0f;
    // // biquadConfig.q = 8.0f;
    // engine.CreateBiquadFilter(biquadFilter, BiquadFilterType::LowPass, biquadConfig);
    // engine.AttachEffectToBus(biquadFilter, "Music", 0);
    //
    // SoundHandle musicSound;
    // engine.LoadSoundAsync(musicSound, SoundType::Stream, "assets/Faouzia - UNETHICAL.ogg");
    //
    // PlaybackHandle musicPlayback;
    // engine.CreatePlayback(musicPlayback, musicSound);
    // engine.RoutePlayback(musicPlayback, "Music");
    //
    // engine.Play(musicPlayback);
    //
    // while (true) {
    //     engine.Update();
    // }
    //
    // engine.Shutdown();

    TestInterface();

    return 0;
}