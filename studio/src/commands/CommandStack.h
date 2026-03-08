#pragma once
#include <vector>
#include <memory>

namespace dalia::studio {

    class Command;

    class CommandStack {
    public:
        CommandStack();
        ~CommandStack();

        void Execute(std::unique_ptr<Command> cmd);
        void Undo();
        void Redo();

        bool CanUndo() const;
        bool CanRedo() const;

    private:
        std::vector<std::unique_ptr<Command>> m_undoStack;
        std::vector<std::unique_ptr<Command>> m_redoStack;
    };
}