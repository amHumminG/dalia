#include "editor/ConsolePanel.h"
#include "core/Logger.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace dalia::studio {

    ConsolePanel::ConsolePanel()
        : Panel("Console") {
    }

    void ConsolePanel::Render() {
        if (ImGui::Button("Clear")) {
            Logger::Clear();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Filters
        ImGui::Checkbox("Engine Logs", &m_showEngineLogs);

        ImGui::Checkbox("Debug", &m_showDebug);
        ImGui::Checkbox("Info", &m_showInfo);
        ImGui::Checkbox("Warnings", &m_showWarnings);
        ImGui::Checkbox("Errors", &m_showErrors);

        ImGui::Separator();

        // Logs Display
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_None);
        {
            std::lock_guard<std::mutex> lock(Logger::GetMutex()); // For studio thread safety in case we log from I/O thread
            const auto& history = Logger::GetHistory();

            static size_t lastSize = 0;
            if (history.size() > lastSize) {
                if (m_autoScroll) m_scrollToBottom = true;
                lastSize = history.size();
            }

            for (const auto& entry : history) {
                if (entry.level == LogLevel::None) continue; // Invalid entry

                if (!m_showEngineLogs && entry.system == LogSystem::Engine) continue;

                if (!m_showDebug && entry.level == LogLevel::Debug) continue;
                if (!m_showInfo && entry.level == LogLevel::Info) continue;
                if (!m_showWarnings && entry.level == LogLevel::Warning) continue;
                if (!m_showErrors && (entry.level == LogLevel::Error || entry.level == LogLevel::Critical)) continue;

                const char* sysStr = (entry.system == LogSystem::Engine) ? "ENGINE" : "STUDIO";
                const char* levelStr = "NONE";
                ImVec4 color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Default Grey

                switch (entry.level) {
                case LogLevel::Debug:
                    levelStr = "DEBUG";
                    color = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
                    break;
                case LogLevel::Info:
                    levelStr = "INFO";
                    color = ImVec4(0.4f, 0.8f, 0.5f, 1.0f);
                    break;
                case LogLevel::Warning:
                    levelStr = "WARN";
                    color = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
                    break;
                case LogLevel::Error:
                    levelStr = "ERROR";
                    color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                    break;
                case LogLevel::Critical:
                    levelStr = "CRIT";
                    color = ImVec4(1.0f, 0.2f, 0.4f, 1.0f);
                    break;
                }

                ImGui::PushTextWrapPos(0.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::Text("[%s %s]", sysStr, levelStr);
                ImGui::PopStyleColor();

                ImGui::SameLine();

                ImGui::Text("%s: %s", entry.category.c_str(), entry.message.c_str());

                ImGui::PopTextWrapPos();
            }

            if (m_scrollToBottom) {
                ImGui::SetScrollHereY(1.0f);
                m_scrollToBottom = false;
            }
        }
        ImGui::EndChild();
    }
}
