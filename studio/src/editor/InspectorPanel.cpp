#include "editor/InspectorPanel.h"
#include "commands/ModifyAssetCommand.h"
#include "imgui.h"

namespace dalia::studio {

    InspectorPanel::InspectorPanel(Project& project, SelectionContext& selectionContext, CommandStack& commands)
        : Panel("Inspector"), m_project(project), m_selection(selectionContext), m_commands(commands) {
    }

    void InspectorPanel::Render() {
        if (!m_selection.HasSelection()) {
            ImGui::TextDisabled("No Asset Selected");
            return;
        }

        SoundAsset* asset = m_project.GetAsset(m_selection.selectedId);
        if (!asset) {
            m_selection.Clear();
            return;
        }

        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "ASSET: %s", asset->name.c_str());
        ImGui::Separator();

        // Display id
        ImGui::BeginDisabled();
        int id = static_cast<int>(asset->id);
        ImGui::InputInt("ID", &id);
        ImGui::EndDisabled();

        ImGui::Spacing();

        // Properties
        DrawFloatProperty("Volume", &asset->volume, 0.0f, 1.0f);

        // TODO: When we integrate the engine, it will look something like this instead:
        // DrawFloatProperty("Volume", &asset->volume, 0.0f, 1.0f
        //     [id]float value { dalia::Engine::SetVolume(id, value); }
        // );

        DrawFloatProperty("Pitch", &asset->pitch, 0.1f, 2.0f);
        DrawBoolProperty("Looping", &asset->isLooping);

        ImGui::TextDisabled("%s", asset->filePath.c_str());
    }

    void InspectorPanel::DrawFloatProperty(const char* label, float* valuePtr, float min, float max,
        std::function<void(float)> callback) {
        static float startValue = 0.0f;

        if (ImGui::IsItemActivated()) {
            startValue = *valuePtr;
        }

        if (ImGui::SliderFloat(label, valuePtr, min, max)) {
            // Probably do nothing here (we probably don't want to send a command for each update here?)
        }

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            float newValue = *valuePtr;
            *valuePtr = startValue;

            auto cmd = std::make_unique<ModifyAssetCommand<float>>(
                valuePtr,
                newValue,
                label,
                callback
            );
            m_commands.Execute(std::move(cmd));
        }
    }

    void InspectorPanel::DrawBoolProperty(const char* label, bool* valuePtr, std::function<void(float)> callback) {
        bool startValue = *valuePtr;

        if (ImGui::Checkbox(label, valuePtr)) {
            bool newValue = *valuePtr;
            *valuePtr = startValue;

            auto cmd = std::make_unique<ModifyAssetCommand<bool>>(
                valuePtr,
                newValue,
                label,
                callback
            );
            m_commands.Execute(std::move(cmd));
        }
    }
}
