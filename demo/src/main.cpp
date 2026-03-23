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
};

void TestInterface() {
     // --- 1. Initialize Raylib and ImGui ---
    InitWindow(1000, 1000, "Dalia Engine - Audio Testbed");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    // --- 2. Initialize Dalia Engine ---
    Engine engine;
    EngineConfig config;
    config.logLevel = LogLevel::Debug;
    engine.Init(config);

    // --- 3. UI State Variables ---
    char assetPathInput[256] = "assets/spiderman.ogg";

    // UNIFIED LISTS!
    std::vector<LoadedAsset> loadedAssets;
    std::vector<PlaybackInstance> playbacks;

    int selectedAssetIdx = -1;
    int selectedPlaybackIdx = -1;

    Result lastResult = Result::Ok;

    // --- 4. Main Game Loop ---
    while (!WindowShouldClose()) {
        // Engine tick processes events, callbacks, and garbage collection
        engine.Update();

        BeginDrawing();
        ClearBackground(DARKGRAY);
        rlImGuiBegin();

        // Main UI Window
        ImGui::SetNextWindowSize(ImVec2(960, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Engine Inspector", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Last Engine API Result: %d", static_cast<int>(lastResult));
        ImGui::Separator();

        // --- SPLIT INTO TWO COLUMNS ---
        ImGui::Columns(2, "MainColumns");

        // ==========================================
        // LEFT COLUMN: ASSET MANAGEMENT
        // ==========================================
        ImGui::TextDisabled("ASSET REGISTRY");
        ImGui::Spacing();

        ImGui::InputText("Filepath", assetPathInput, 256);
        ImGui::Spacing();

        // Unified Loading
        if (ImGui::Button("Load to RAM (Resident)", ImVec2(180, 30))) {
            SoundHandle h;
            lastResult = engine.LoadSoundAsync(h, SoundType::Resident, assetPathInput);
            if (lastResult == Result::Ok) {
                loadedAssets.push_back({assetPathInput, h});
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load to Disk (Stream)", ImVec2(180, 30))) {
            SoundHandle h;
            lastResult = engine.LoadSoundAsync(h, SoundType::Stream, assetPathInput);
            if (lastResult == Result::Ok) {
                loadedAssets.push_back({assetPathInput, h});
            }
        }

        ImGui::Spacing();

        // Unified Asset List
        ImGui::BeginChild("AssetList", ImVec2(0, 200), true);
        for (int i = 0; i < loadedAssets.size(); i++) {
            SoundHandle h = loadedAssets[i].handle;
            std::string typeTag = (h.GetType() == SoundType::Resident) ? "[RAM] " : "[DISK] ";
            std::string label = typeTag + loadedAssets[i].path + " (UUID: " + std::to_string(h.GetUUID()) + ")";

            if (ImGui::Selectable(label.c_str(), selectedAssetIdx == i)) {
                selectedAssetIdx = i;
            }
        }
        ImGui::EndChild();

// Unified Asset Controls
        if (ImGui::Button("Unload Selected Asset") && selectedAssetIdx >= 0 && selectedAssetIdx < loadedAssets.size()) {
            lastResult = engine.UnloadSound(loadedAssets[selectedAssetIdx].handle);
            loadedAssets.erase(loadedAssets.begin() + selectedAssetIdx);
            selectedAssetIdx = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Playback") && selectedAssetIdx >= 0 && selectedAssetIdx < loadedAssets.size()) {
            PlaybackHandle pHandle;
            lastResult = engine.CreatePlayback(pHandle, loadedAssets[selectedAssetIdx].handle);
            if (lastResult == Result::Ok) {
                std::string typeTag = (loadedAssets[selectedAssetIdx].handle.GetType() == SoundType::Resident) ? "[RAM] " : "[STR] ";
                playbacks.push_back({typeTag + loadedAssets[selectedAssetIdx].path, pHandle});
            }
        }

        ImGui::NextColumn();

        // ==========================================
        // RIGHT COLUMN: PLAYBACK VOICES
        // ==========================================
        ImGui::TextDisabled("VOICE MIXER");
        ImGui::Spacing();

        ImGui::Text("Active Playback Instances");
        ImGui::BeginChild("PlaybackList", ImVec2(0, 300), true);
        for (int i = 0; i < playbacks.size(); i++) {
            // We can safely prune dead handles from the UI if we want to get fancy,
            // but for now, we just display them.
            std::string label = playbacks[i].name + " (Handle: " + std::to_string(playbacks[i].handle.GetUUID()) + ")";
            if (ImGui::Selectable(label.c_str(), selectedPlaybackIdx == i)) {
                selectedPlaybackIdx = i;
            }
        }
        ImGui::EndChild();

        if (selectedPlaybackIdx >= 0 && selectedPlaybackIdx < playbacks.size()) {
            PlaybackHandle currentHandle = playbacks[selectedPlaybackIdx].handle;

            // Unified Transport Controls!
            if (ImGui::Button("Play", ImVec2(80, 40))) {
                lastResult = engine.Play(currentHandle);
            }
            ImGui::SameLine();
            if (ImGui::Button("Pause", ImVec2(80, 40))) {
                lastResult = engine.Pause(currentHandle);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(80, 40))) {
                lastResult = engine.Stop(currentHandle);
            }
        } else {
            ImGui::TextDisabled("Select a playback instance to control it.");
        }

        ImGui::Columns(1);
        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
    }

    // --- 5. Cleanup ---
    engine.Deinit();
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

    engine.CreateBus("Music", "Master");

    SoundHandle spidermanSound;
    engine.LoadSoundAsync(spidermanSound, SoundType::Resident, "assets/spiderman.ogg", SpidermanFinishedLoading);

    PlaybackHandle spidermanPlayback;
    engine.CreatePlayback(spidermanPlayback, spidermanSound, SpidermanFinishedPlaying);
    engine.RoutePlayback(spidermanPlayback, "Music");

    engine.Play(spidermanPlayback);

    while (true) {
        engine.Update();
    }

    // engine.Update();
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    // engine.Update();
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    // engine.Stop(spidermanPlayback);
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    // engine.Update();
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    // engine.Update();
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    engine.Deinit();

    return 0;
}