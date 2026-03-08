#include "app/Application.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui_internal.h"

// Core
#include "core/Logger.h"

// Panels
#include "editor/BrowserPanel.h"
#include "editor/InspectorPanel.h"
#include "editor/ConsolePanel.h"

#include <iostream>

namespace dalia::studio {

    Application::Application(int width, int height, const std::string& title) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
        InitWindow(width, height, title.c_str());
        Image icon = LoadImage("assets/images/dalia_icon.png");
        if (icon.data) SetWindowIcon(icon);
        else UnloadImage(icon);
        SetTargetFPS(60);

        rlImGuiSetup(true); // ImGui dark mode

        // Enable docking
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "dalia_studio_layout.ini";

        // UI font
        ImFont* font = io.Fonts->AddFontFromFileTTF("assets/fonts/Cousine-Regular.ttf", 14.0f);
        if (!font) std::cout << "Unable to load font!" << std::endl;
        else io.FontDefault = font;

        SetupTheme();

        // Setup project (this should be done in CreateProject or LoadProject later on)
        m_project = std::make_unique<Project>();
        m_commands = std::make_unique<CommandStack>();

        // Setup panels
        m_panels.push_back(std::make_unique<BrowserPanel>(*m_project, m_selectionContext));
        m_panels.push_back(std::make_unique<InspectorPanel>(*m_project, m_selectionContext, *m_commands));
        m_panels.push_back(std::make_unique<ConsolePanel>());
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

    void Application::SetupTheme() {
        auto& style = ImGui::GetStyle();

        // --- GEOMETRY ---
        style.WindowRounding    = 4.0f; // Slight rounding
        style.FrameRounding     = 2.0f; // Soften buttons/inputs
        style.PopupRounding     = 2.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding      = 2.0f;
        style.TabRounding       = 4.0f;

        style.ItemSpacing       = ImVec2(8, 4);
        style.FramePadding      = ImVec2(6, 4);
        style.WindowPadding     = ImVec2(8, 8);
        style.ScrollbarSize     = 12.0f; // Slightly thinner scrollbars

        // --- COLORS ---
        ImVec4* colors = style.Colors;

        // Base Backgrounds (Dark Grey)
        colors[ImGuiCol_WindowBg]       = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg]        = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PopupBg]        = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_Border]         = ImVec4(0.25f, 0.25f, 0.25f, 0.50f);

        // Headers (The bars at the top of windows/collapsing headers)
        colors[ImGuiCol_Header]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_HeaderHovered]  = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderActive]   = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

        // Titles (The draggable window bar)
        colors[ImGuiCol_TitleBg]        = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_TitleBgActive]  = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]= ImVec4(0.00f, 0.00f, 0.00f, 0.51f);

        // Buttons (The workhorses)
        colors[ImGuiCol_Button]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_ButtonHovered]  = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_ButtonActive]   = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

        ImVec4 accentColor              = ImVec4(0.85f, 0.55f, 0.20f, 1.00f);

        colors[ImGuiCol_CheckMark]          = accentColor;
        colors[ImGuiCol_SliderGrab]         = accentColor;
        colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.95f, 0.65f, 0.30f, 1.00f); // Brighter when dragging

        // Tabs (Docking)
        colors[ImGuiCol_Tab]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TabHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_TabActive]          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TabUnfocused]       = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);

        // Text
        colors[ImGuiCol_Text]           = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TextDisabled]   = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        // Frame Backgrounds
        colors[ImGuiCol_FrameBg]        = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_FrameBgActive]  = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    }

    void Application::InitEngine() {
        m_engine = std::make_unique<Engine>();
        dalia::EngineConfig config;
        config.logLevel = dalia::LogLevel::Debug;
        config.logCallback = &Logger::LogEngine;
        // m_engine->Init(config);
        Logger::Log(LogLevel::Debug, "Constructor", "Initialized internal engine.");
        Logger::Log(LogLevel::Info, "Constructor", "Initialized internal engine.");
        Logger::Log(LogLevel::Warning, "Constructor", "Initialized internal engine.");
        Logger::Log(LogLevel::Error, "Constructor", "Initialized internal engine.");
        Logger::Log(LogLevel::Critical, "Constructor", "Initialized internal engine.");
    }

    void Application::Update() {
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
            m_commands->Undo();
            std::cout << "Undo" << std::endl;
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) {
            m_commands->Redo();
            std::cout << "Redo" << std::endl;
        }
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

        // Panels
        for (auto& panel : m_panels) {
            panel->OnRender();
        }
    }
}
