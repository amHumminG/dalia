#include "CommandQueue.h"
#include "Logger.h"

namespace placeholder_name {

	CommandQueue::CommandQueue(size_t commandCapacity) 
		: m_buffer(commandCapacity) {
		m_stagingArea.reserve(commandCapacity);
	}

	void CommandQueue::Enqueue(const AudioCommand& command) {
		m_stagingArea.push_back(command);
	}

	void CommandQueue::Flush() {
		size_t commandsPushed = 0;
		for (const auto& command : m_stagingArea) {
			if (!m_buffer.Push(command)) {
				Logger::Log(LogLevel::Warning, "CommandQueue", "Unable to push all commands this frame. Internal buffer is Full.");
				break;
			}
			else {
				commandsPushed++;
			}
		}
		// Remove all commands that got pushed
		if (commandsPushed > 0) {
			m_stagingArea.erase(m_stagingArea.begin(), m_stagingArea.begin() + commandsPushed);
		}
	}

	bool CommandQueue::Pop(AudioCommand& command) {
		if (!m_buffer.Pop(command)) {
			return false;
		}
		return true;
	}
}