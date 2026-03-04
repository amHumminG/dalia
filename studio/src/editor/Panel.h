#pragma once
#include <string>

namespace dalia::studio {

    class Panel {
    public:
        Panel(const std::string& title) : m_title(title) {}
        virtual ~Panel();

        virtual void OnRender() = 0;

        void Open() { m_isOpen = true; }
        void Close() { m_isOpen = false; }
        bool IsOpen() const { return m_isOpen; }

    private:
        std::string m_title;
        bool m_isOpen;
    };
}