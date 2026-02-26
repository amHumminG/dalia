#include "AudioCommandQueue.h"
#include "core/Logger.h"

namespace dalia {

	AudioCommandQueue::AudioCommandQueue(size_t commandCapacity)
		: m_buffer(commandCapacity) {
		m_stagingArea.reserve(commandCapacity);
	}

	void AudioCommandQueue::Enqueue(const AudioCommand& command) {
		m_stagingArea.push_back(command);
	}

	void AudioCommandQueue::Dispatch() {
		size_t commandsPushed = 0;
		for (const auto& command : m_stagingArea) {
			if (!m_buffer.Push(command)) {
				Logger::Log(LogLevel::Warning, "AudioCommandQueue", "Unable to push all commands this frame. Internal buffer is Full.");
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

	bool AudioCommandQueue::Pop(AudioCommand& command) {
		if (!m_buffer.Pop(command)) {
			return false;
		}
		return true;
	}
}