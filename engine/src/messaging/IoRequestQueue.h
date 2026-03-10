#pragma once
#include "core/MPSCRingBuffer.h"

namespace dalia {

    static constexpr size_t MAX_IO_PATH_SIZE = 256;

    struct StreamingContext;
    struct Soundbank;

    struct IoRequest {
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
                char path[MAX_IO_PATH_SIZE];
            } stream;

            struct {
                uint32_t bankId; // TODO: This should not be pointer (Change to an index in a soundbank table)
                uint32_t assetId; // Maybe this should be a filepath? (if not we need a lookup table)
            } bank;
        } data = {};

        static IoRequest PrepareStream(uint32_t poolIndex, const char* path) {
            IoRequest req;
            req.type = Type::PrepareStream;
            req.data.stream.poolIndex = poolIndex;
            strncpy_s(req.data.stream.path, MAX_IO_PATH_SIZE, path, _TRUNCATE);
            return req;
        }

        static IoRequest ReleaseStream(uint32_t poolIndex) {
            IoRequest req;
            req.type = Type::ReleaseStream;
            req.data.stream.poolIndex = poolIndex;
            return req;
        }

        static IoRequest RefillStreamBuffer(uint32_t poolIndex, uint32_t generation, uint32_t bufferIndex) {
            IoRequest req;
            req.type = Type::RefillStreamBuffer;
            req.data.stream.poolIndex = poolIndex;
            req.data.stream.generation = generation;
            req.data.stream.bufferIndex = bufferIndex;
            return req;
        }

        static IoRequest LoadBank(uint32_t bankId, const uint32_t assetID) {
            IoRequest req;
            req.type = Type::LoadSoundbank;
            req.data.bank.bankId = bankId;
            req.data.bank.assetId = assetID;
            return req;
        }
    };

    class IoRequestQueue {
    public:
        IoRequestQueue(size_t capacity);
        ~IoRequestQueue() = default;

        bool Push(const IoRequest& request);

        bool Pop(IoRequest& request);

    private:
        MPSCRingBuffer<IoRequest> m_buffer;
    };


}