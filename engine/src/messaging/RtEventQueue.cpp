#include "messaging/RtEventQueue.h"

namespace dalia {

	RtEventQueue::RtEventQueue(size_t eventCapacity)
		: m_buffer(eventCapacity) {}

	bool RtEventQueue::Push(const RtEvent& event) {
		return m_buffer.Push(event);
	}

	bool RtEventQueue::Pop(RtEvent& event) {
		return m_buffer.Pop(event);
	}
}