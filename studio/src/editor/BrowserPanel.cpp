#include "editor/BrowserPanel.h"
#include "imgui_internal.h"
#include "core/Logger.h"
#include "FileExplorer.cpp"

namespace dalia::studio {

    BrowserPanel::BrowserPanel(Project& project, SelectionContext& selection)
        :Panel("Asset Browser"), m_project(project), m_selectionContext(selection) {
    }

    BrowserPanel::~BrowserPanel() = default;

    void BrowserPanel::Render() {
        if (ImGui::Button("Import Sound")) {
            const std::vector<std::string> filePaths = dalia::studio::OpenFileExplorer();
            if (!filePaths.empty()) {
                for (const std::string& filePath : filePaths) {
                    m_project.ImportSound(filePath);
                    Logger::Log(LogLevel::Info, "Browser", "Imported asset");
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            m_selectionContext.Clear();
        }

        ImGui::Separator();

        ImGui::BeginChild("Asset List");
        for (const auto& assetPtr : m_project.GetAllAssets()) {
            // X button
            std::string label = std::string("X##" + std::to_string(assetPtr->id));
            if (ImGui::Button(label.c_str(), ImVec2(20, 20))) {
                m_project.RemoveAsset(assetPtr->id);
                break;
            }

            // Selectable asset
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAvailWidth;
            bool isSelected = m_selectionContext.IsSelected(assetPtr->id);
            label = std::string(assetPtr->name + "##" + std::to_string(assetPtr->id));
            ImGui::SameLine();
            if (ImGui::Selectable(label.c_str(), isSelected, flags)) {
                m_selectionContext.Select(assetPtr->id);
            }
        }
        ImGui::EndChild();
    }
}
