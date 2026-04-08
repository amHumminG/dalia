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
            SeekStream,
        };
        Type type = Type::None;

    	uint32_t index;
    	uint32_t gen;

        union {
            struct {
                char filepath[MAX_IO_PATH_SIZE];
            } streamPrep;

            struct {
                uint32_t bufferIndex;
            } streamRefill;

            struct {
                uint32_t seekFrame;
            } streamSeek;
        } data = {};

        static IoStreamRequest PrepareStream(uint32_t index, uint32_t gen, const char* path) {
            IoStreamRequest req;
            req.type = Type::PrepareStream;
            req.index = index;
        	req.gen = gen;
            strncpy_s(req.data.streamPrep.filepath, MAX_IO_PATH_SIZE, path, _TRUNCATE);
            return req;
        }

        static IoStreamRequest ReleaseStream(uint32_t index, uint32_t gen) {
            IoStreamRequest req;
            req.type = Type::ReleaseStream;
            req.index = index;
        	req.gen = gen;
            return req;
        }

        static IoStreamRequest RefillStreamBuffer(uint32_t index, uint32_t gen, uint32_t bufferIndex) {
            IoStreamRequest req;
            req.type = Type::RefillStreamBuffer;
            req.index = index;
            req.gen = gen;
            req.data.streamRefill.bufferIndex = bufferIndex;
            return req;
        }

        static IoStreamRequest SeekStream(uint32_t index, uint32_t gen, uint32_t seekFrame) {
            IoStreamRequest req{};
            req.type = Type::SeekStream;
            req.index = index;
            req.gen = gen;
            req.data.streamSeek.seekFrame = seekFrame;
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
