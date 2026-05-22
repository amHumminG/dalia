#include "messaging/IoLoadRequestQueue.h"

namespace dalia {

    IoLoadRequestQueue::IoLoadRequestQueue(size_t capacity)
        : m_buffer(capacity) {
    }

    bool IoLoadRequestQueue::Push(const IoLoadRequest& request) {
        return m_buffer.Push(request);
    }

    bool IoLoadRequestQueue::Pop(IoLoadRequest& request) {
        return m_buffer.Pop(request);
    }
}
