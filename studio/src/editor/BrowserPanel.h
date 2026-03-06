#pragma once
#include "editor/Panel.h"
#include "project/Project.h"
#include "editor/SelectionContext.h"

namespace dalia::studio {

    class BrowserPanel : public Panel {
    public:
        BrowserPanel(Project& project, SelectionContext& selection);
        ~BrowserPanel() override;

    protected:
        void Render() override;

    private:
        Project& m_project;
        SelectionContext& m_selectionContext;
    };
}