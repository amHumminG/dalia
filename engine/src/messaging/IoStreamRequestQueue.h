#pragma once
#include "core/Constants.h"
#include "core/MPSCRingBuffer.h"

namespace dalia {

    struct IoStreamRequest {
        enum class Type {
            None,
            PrepareStream,
            ReleaseStream,
            RefillStreamBuffer,
            LoadSoundbank
        };
        Type type = Type::None;

        union {
            struct {
                uint32_t poolIndex;
                uint32_t  generation;
                uint32_t bufferIndex;
                char filepath[MAX_IO_PATH_SIZE];
            } stream;
        } data = {};

        static IoStreamRequest PrepareStream(uint32_t poolIndex, const char* path) {
            IoStreamRequest req;
            req.type = Type::PrepareStream;
            req.data.stream.poolIndex = poolIndex;
            strncpy_s(req.data.stream.filepath, MAX_IO_PATH_SIZE, path, _TRUNCATE);
            return req;
        }

        static IoStreamRequest ReleaseStream(uint32_t poolIndex) {
            IoStreamRequest req;
            req.type = Type::ReleaseStream;
            req.data.stream.poolIndex = poolIndex;
            return req;
        }

        static IoStreamRequest RefillStreamBuffer(uint32_t poolIndex, uint32_t generation, uint32_t bufferIndex) {
            IoStreamRequest req;
            req.type = Type::RefillStreamBuffer;
            req.data.stream.poolIndex = poolIndex;
            req.data.stream.generation = generation;
            req.data.stream.bufferIndex = bufferIndex;
            return req;
        }
    };

    class IoStreamRequestQueue {
    public:
        IoStreamRequestQueue(size_t capacity);
        ~IoStreamRequestQueue() = default;

        bool Push(const IoStreamRequest& request);

        bool Pop(IoStreamRequest& request);

    private:
        MPSCRingBuffer<IoStreamRequest> m_buffer;
    };


}
