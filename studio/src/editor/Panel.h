#pragma once
#include <string>
#include "imgui.h"

namespace dalia::studio {

    class Panel {
    public:
        Panel(const std::string& title) : m_title(title) {}
        virtual ~Panel();

        virtual void OnRender() {
            if (!m_isOpen) return;

            if (ImGui::Begin(m_title.c_str(), &m_isOpen)) {
                Render();
            }
        };

        void Open() { m_isOpen = true; }
        void Close() { m_isOpen = false; }
        bool IsOpen() const { return m_isOpen; }

    private:
        virtual void Render();

        std::string m_title;
        bool m_isOpen;
    };
}