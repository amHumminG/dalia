#include "EventQueue.h"

namespace placeholder_name {

	EventQueue::EventQueue(size_t eventCapacity) 
		: m_buffer(eventCapacity) {}

	bool EventQueue::Push(const AudioEvent& event) {
		return m_buffer.Push(event);
	}

	bool EventQueue::Poll(AudioEvent& event) {
		return m_buffer.Pop(event);
	}
}