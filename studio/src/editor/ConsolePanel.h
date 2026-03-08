#pragma once
#include "Panel.h"

namespace dalia::studio {

    class ConsolePanel : public Panel {
    public:
        ConsolePanel();
        ~ConsolePanel() = default;

    protected:
        void Render();

    private:
        bool m_scrollToBottom = true;
        bool m_autoScroll = true;

        // Filters
        bool m_showEngineLogs = true;

        bool m_showDebug = false;
        bool m_showInfo = true;
        bool m_showWarnings = true;
        bool m_showErrors = true;
    };
}