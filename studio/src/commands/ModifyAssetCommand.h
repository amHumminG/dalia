#pragma once
#include "commands/Command.h"
#include <functional>

namespace dalia::studio {

    template <typename T>
    class ModifyAssetCommand : public Command {
    public:
        ModifyAssetCommand(T* ptrToValue, T newValue, const std::string& propertyName, std::function<void(T)> callback = nullptr)
            : m_target(ptrToValue), m_newValue(newValue), m_name(propertyName), m_callback(callback) {
            m_oldValue = *m_target;
        }

        void Execute() override {
            *m_target = m_newValue;
            if (m_callback) m_callback(m_newValue);
        }

        void Unexecute() override {
            *m_target = m_oldValue;
            if (m_callback) m_callback(m_oldValue);
        }

        std::string GetName() const override {
            return "Modify " + m_name;
        }

    private:
        T* m_target; // Address of the variable to change
        T m_newValue;
        T m_oldValue;
        std::string m_name;

        std::function<void(T)> m_callback;
    };
}
