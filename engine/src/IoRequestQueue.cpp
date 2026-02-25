#include "IoRequestQueue.h"

namespace dalia {

    IoRequestQueue::IoRequestQueue(size_t capacity)
        : m_buffer(capacity) {}

    bool IoRequestQueue::Push(const IoRequest& request) {
        return m_buffer.Push(request);
    }

    bool IoRequestQueue::Pop(IoRequest& request) {
        return m_buffer.Pop(request);
    }
}


