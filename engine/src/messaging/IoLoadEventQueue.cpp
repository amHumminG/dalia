#include "messaging/IoLoadEventQueue.h"

namespace dalia {

    IoLoadEventQueue::IoLoadEventQueue(size_t capacity)
        : m_buffer(capacity) {
    }

    bool IoLoadEventQueue::Push(const IoLoadEvent& event) {
        return m_buffer.Push(event);
    }

    bool IoLoadEventQueue::Pop(IoLoadEvent& event) {
        return m_buffer.Pop(event);
    }
}
