#include "MixerMessaging.h"

#include "core/Logger.h"

namespace dalia {

	MixerCommandQueue::MixerCommandQueue(size_t commandCapacity)
		: m_buffer(commandCapacity) {
		m_stagingArea.reserve(commandCapacity);
	}

	void MixerCommandQueue::Enqueue(const MixerCommand& command) {
		m_stagingArea.push_back(command);
	}

	void MixerCommandQueue::Dispatch() {
		size_t commandsPushed = 0;
		for (const auto& command : m_stagingArea) {
			if (!m_buffer.Push(command)) {
				DALIA_LOG_WARN(LOG_CTX_MESSAGING, "Unable to push all mixer commands this frame. Command queue is at capacity.");
				break;
			}
			commandsPushed++;
		}

		// Remove pushed commands from staging area
		if (commandsPushed == m_stagingArea.size()) {
			// All commands were pushed
			m_stagingArea.clear();
		}
		else if (commandsPushed > 0) {
			// There are still remaining commands in the staging area
			m_stagingArea.erase(m_stagingArea.begin(), m_stagingArea.begin() + commandsPushed);
		}
	}

	bool MixerCommandQueue::Pop(MixerCommand& command) {
		if (!m_buffer.Pop(command)) {
			return false;
		}
		return true;
	}
}