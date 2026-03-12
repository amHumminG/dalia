#pragma once
#include "core/SPSCRingBuffer.h"

namespace dalia {

    struct IoLoadRequest {
        enum class Type {
            None,

            LoadSoundFromFile,

        } type = Type::None;

        union {
            struct {
                // ResidentSound* sound;
                char filepath[256];
            } LoadSoundFromFile;

        } data;

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
