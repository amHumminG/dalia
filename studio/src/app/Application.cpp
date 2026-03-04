#include "app/Application.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui_internal.h"

namespace dalia::studio {

    Application::Application(int width, int height, const std::string& title) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
        InitWindow(width, height, title.c_str());
        SetTargetFPS(60);

        rlImGuiSetup(true); // ImGui dark mode

        // Enable docking
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "dalia_studio_layout.ini";
    }

    Application::~Application() {
        rlImGuiShutdown();
        CloseWindow();
    }

    void Application::Run() {
        while (!WindowShouldClose()) {
            Update();
            Render();
        }
    }

    void Application::Update() {
    }

    void Application::Render() {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        rlImGuiBegin();

        RenderUI();

        rlImGuiEnd();
        EndDrawing();
    }

    void Application::RenderUI() {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // Example: The Main Menu Bar at the top
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) CloseWindow();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Reset Layout", nullptr);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Example: A dummy tool panel
        ImGui::Begin("Asset Browser");
        ImGui::Text("Vi support players are gay");
        ImGui::End();

        // Example: Another panel to test docking
        ImGui::Begin("Inspector");
        ImGui::Text("Selection properties");
        ImGui::End();

        // Panels
        for (auto& panel : m_panels) {
            panel->OnRender();
        }
    }
}
