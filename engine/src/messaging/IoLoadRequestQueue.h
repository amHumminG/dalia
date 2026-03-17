#pragma once
#include "dalia/audio/SoundHandle.h"
#include "core/Constants.h"
#include "core/SPSCRingBuffer.h"
#include <cstdint>

namespace dalia {

    struct IoLoadRequest {
        enum class Type {
            None,

            LoadSound,
            // LoadBank

        } type = Type::None;

        uint32_t requestId;

        union {
            struct {
                uint64_t resourceHandleUuid;
                char filepath[MAX_IO_PATH_SIZE];
            } soundFromFile;

        } data = {};

        static IoLoadRequest LoadSound(uint32_t reqId, SoundHandle handle, const char* filepath) {
            IoLoadRequest req;
            req.type = Type::LoadSound;
            req.requestId = reqId;

            req.data.soundFromFile.resourceHandleUuid = handle.GetUUID();
            strncpy_s(req.data.soundFromFile.filepath, MAX_IO_PATH_SIZE, filepath, _TRUNCATE);

            return req;
        }
    };

    class IoLoadRequestQueue {
    public:
        IoLoadRequestQueue(size_t capacity);
        ~IoLoadRequestQueue() = default;

        bool Push(const IoLoadRequest& request);
        bool Pop(IoLoadRequest& request);

    private:
        SPSCRingBuffer<IoLoadRequest> m_buffer;
    };
}
