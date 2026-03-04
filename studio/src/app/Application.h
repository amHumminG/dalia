#pragma once
#include "editor/Panel.h"

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
        void Update();
        void Render();
        void RenderUI();

        std::vector<std::unique_ptr<Panel>> m_panels;
    };
}
