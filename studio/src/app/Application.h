#pragma once
#include "project/Project.h"
#include "editor/Panel.h"
#include "editor/SelectionContext.h"
#include "commands/CommandStack.h"

#include "dalia.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace dalia::studio {

    class Application {
    public:
        Application(int width, int height, const std::string& title);
        ~Application();

        static Application& Get();
        static Application* GetInstance();

        void SubmitToMainThread(std::function<void()> function);

        void Run();

    private:
        void SetupTheme();
        void InitEngine();
        void Update();
        void Render();
        void RenderUI();

        void ExecuteMainThreadQueue();

        std::unique_ptr<Engine> m_engine;
        SelectionContext m_selectionContext;
        std::unique_ptr<Project> m_project;             // Model
        std::vector<std::unique_ptr<Panel>> m_panels;   // View
        std::unique_ptr<CommandStack> m_commands;       // Controller

        std::vector<std::function<void()>> m_mainThreadQueue;
        std::mutex m_mainThreadQueueMutex;
    };
}
