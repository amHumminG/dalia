#include "commands/CommandStack.h"
#include "commands/Command.h"

namespace dalia::studio {

    CommandStack::CommandStack() = default;
    CommandStack::~CommandStack() = default;

    void CommandStack::Execute(std::unique_ptr<Command> cmd) {
        cmd->Execute();
        m_undoStack.push_back(std::move(cmd));

        m_redoStack.clear();
    }

    void CommandStack::Undo() {
        if (m_undoStack.empty()) return;
        auto cmd = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        cmd->Unexecute();
        m_redoStack.push_back(std::move(cmd));
    }

    void CommandStack::Redo() {
        if (m_redoStack.empty()) return;
        auto cmd = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        cmd->Execute();
        m_undoStack.push_back(std::move(cmd));
    }

    bool CommandStack::CanUndo() const {
        return !m_undoStack.empty();
    }

    bool CommandStack::CanRedo() const {
        return !m_redoStack.empty();
    }
}
