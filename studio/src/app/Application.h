#pragma once
#include "project/Project.h"
#include "editor/Panel.h"
#include "editor/SelectionContext.h"
#include "commands/CommandStack.h"

#include "dalia/AudioEngine.h"

#include <string>
#include <vector>
#include <memory>

namespace dalia::studio {

    class Application {
    public:
        Application(int width, int height, const std::string& title);
        ~Application();

        void Run();

    private:
        void SetupTheme();
        void InitEngine();
        void Update();
        void Render();
        void RenderUI();

        std::unique_ptr<AudioEngine> m_engine;
        SelectionContext m_selectionContext;
        std::unique_ptr<Project> m_project;             // Model
        std::vector<std::unique_ptr<Panel>> m_panels;   // View
        std::unique_ptr<CommandStack> m_commands;       // Controller
    };
}
