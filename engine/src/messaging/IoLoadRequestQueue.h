#pragma once
#include "core/Constants.h"
#include "core/SPSCRingBuffer.h"
#include <cstdint>

namespace dalia {

    struct IoLoadRequest {
        enum class Type {
            None,

            LoadSoundFromFile,

        } type = Type::None;

        uint32_t requestId;

        union {
            struct {
                // ResidentSound* sound;
                char filepath[MAX_IO_PATH_SIZE];
            } LoadSoundFromFile;

        } data = {};

        static IoLoadRequest LoadSoundFromFile(const char* filepath) {

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
