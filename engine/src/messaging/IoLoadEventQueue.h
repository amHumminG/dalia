#pragma once
#include "dalia/core/Result.h"
#include "core/SPSCRingBuffer.h"
#include "mixer/Voice.h"
#include <cstdint>

namespace dalia {

    struct IoLoadEvent {
        enum class Type {
            None,

            SoundLoaded,
            SoundLoadFailed,
        } type = Type::None;

        uint32_t requestId = 0;
        Result result = Result::Ok;

        uint64_t assetRawId = 0;

        static IoLoadEvent SoundLoaded(uint32_t reqId, uint64_t assetRawId) {
            IoLoadEvent ev;
            ev.type = Type::SoundLoaded;
            ev.requestId = reqId;
            ev.assetRawId = assetRawId;
            return ev;
        }

        static IoLoadEvent SoundLoadFailed(uint32_t reqId, Result error) {
            IoLoadEvent ev;
            ev.type = Type::SoundLoadFailed;
            ev.requestId = reqId;
            ev.result = error;
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