#pragma once
#include "dalia/audio/ResourceHandle.h"
#include "core/Constants.h"
#include "core/SPSCRingBuffer.h"
#include <cstdint>

namespace dalia {

    struct IoLoadRequest {
        enum class Type {
            None,

            LoadResidentSound,
            LoadStreamSound,
            // LoadBank

        } type = Type::None;

        uint32_t requestId;

        union {
            struct {
                uint64_t resourceHandleUuid;
                char filepath[MAX_IO_PATH_SIZE];
            } soundFromFile;

        } data = {};

        static IoLoadRequest LoadResidentSound(uint32_t reqId, ResidentSoundHandle handle, const char* filepath) {
            IoLoadRequest req;
            req.type = Type::LoadResidentSound;
            req.requestId = reqId;

            req.data.soundFromFile.resourceHandleUuid = handle.GetUUID();
            strncpy_s(req.data.soundFromFile.filepath, MAX_IO_PATH_SIZE, filepath, _TRUNCATE);

            return req;
        }

        static IoLoadRequest LoadStreamSound(uint32_t reqId, StreamSoundHandle handle, const char* filepath) {
            IoLoadRequest req;
            req.type = Type::LoadStreamSound;
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
