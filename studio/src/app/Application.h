#pragma once
#include "project/Project.h"
#include "editor/Panel.h"
#include "editor/SelectionContext.h"
#include "commands/CommandStack.h"

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
        void Update();
        void Render();
        void RenderUI();

        SelectionContext m_selectionContext;
        std::unique_ptr<Project> m_project;             // Model
        std::vector<std::unique_ptr<Panel>> m_panels;   // View
        std::unique_ptr<CommandStack> m_commands;       // Controller
    };
}
