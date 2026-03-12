#include "IoStreamRequestQueue.h"

namespace dalia {

    IoStreamRequestQueue::IoStreamRequestQueue(size_t capacity)
        : m_buffer(capacity) {}

    bool IoStreamRequestQueue::Push(const IoStreamRequest& request) {
        return m_buffer.Push(request);
    }

    bool IoStreamRequestQueue::Pop(IoStreamRequest& request) {
        return m_buffer.Pop(request);
    }
}


