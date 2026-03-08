#include "editor/BrowserPanel.h"
#include "imgui_internal.h"
#include "core/Logger.h"

namespace dalia::studio {

    BrowserPanel::BrowserPanel(Project& project, SelectionContext& selection)
        :Panel("Asset Browser"), m_project(project), m_selectionContext(selection) {
    }

    BrowserPanel::~BrowserPanel() = default;

    void BrowserPanel::Render() {
        if (ImGui::Button("Import Sound")) {
            // TODO: Open file explorer window here
            m_project.ImportSound("assets/Zeri.ogg");
            Logger::Log(LogLevel::Info, "Browser", "Imported asset");
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            m_selectionContext.Clear();
        }

        ImGui::Separator();

        ImGui::BeginChild("Asset List");
        for (const auto& assetPtr : m_project.GetAllAssets()) {
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAvailWidth;

            bool isSelected = m_selectionContext.IsSelected(assetPtr->id);

            if (ImGui::Selectable(assetPtr->name.c_str(), isSelected, flags)) {
                m_selectionContext.Select(assetPtr->id);
            }
        }
        ImGui::EndChild();
    }
}
