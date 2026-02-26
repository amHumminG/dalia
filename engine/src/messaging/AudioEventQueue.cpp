#include "AudioEventQueue.h"

namespace dalia {

	AudioEventQueue::AudioEventQueue(size_t eventCapacity)
		: m_buffer(eventCapacity) {}

	bool AudioEventQueue::Push(const AudioEvent& event) {
		return m_buffer.Push(event);
	}

	bool AudioEventQueue::Pop(AudioEvent& event) {
		return m_buffer.Pop(event);
	}
}