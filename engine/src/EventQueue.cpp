#include "EventQueue.h"

namespace dalia {

	EventQueue::EventQueue(size_t eventCapacity) 
		: m_buffer(eventCapacity) {}

	bool EventQueue::Push(const AudioEvent& event) {
		return m_buffer.Push(event);
	}

	bool EventQueue::Pop(AudioEvent& event) {
		return m_buffer.Pop(event);
	}
}