#pragma once
#include "Panel.h"
#include "project/Project.h"
#include "SelectionContext.h"
#include "commands/CommandStack.h"

#include <functional>

namespace dalia::studio {

    class InspectorPanel : public Panel {
    public:
        InspectorPanel(Project& project, SelectionContext& selectionContext, CommandStack& commands);

    protected:
        void Render() override;

    private:
        void DrawFloatProperty(const char* label, float* valuePtr, float min, float max,
            std::function<void(float)> callback = nullptr);

        void DrawBoolProperty(const char* label, bool* valuePtr,
            std::function<void(float)> callback = nullptr);

        Project& m_project;
        SelectionContext& m_selection;
        CommandStack& m_commands;
    };
}