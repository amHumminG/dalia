#pragma once
#include "dalia/core/Result.h"
#include "core/SPSCRingBuffer.h"
#include <cstdint>

namespace dalia {

    struct IoLoadEvent {
        enum class Type {
            None,

            SoundLoadedFromFile,
            SoundLoadFailed,
        } type = Type::None;

        uint32_t requestId;
        Result result; // Eventual error codes

        static IoLoadEvent SoundLoadedFromFile(uint32_t reqId) {
            IoLoadEvent ev;
            ev.type = Type::SoundLoadedFromFile;
            ev.requestId = reqId;
            return ev;
        }
    };

    class IoLoadEventQueue {
    public:
        IoLoadEventQueue(size_t capacity);
        ~IoLoadEventQueue() = default;

        bool Push(const IoLoadEvent& event);
        bool Pop(IoLoadEvent& event);

    private:
        SPSCRingBuffer<IoLoadEvent> m_buffer;
    };

}