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

        uint64_t assetUuid = 0;
        VoiceSourceType sourceType = VoiceSourceType::None;

        static IoLoadEvent SoundLoaded(uint32_t reqId, uint64_t uuid, VoiceSourceType sourceType) {
            IoLoadEvent ev;
            ev.type = Type::SoundLoaded;
            ev.requestId = reqId;
            ev.assetUuid = uuid;
            ev.sourceType = sourceType;
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