#pragma once
#include "core/MPSCRingBuffer.h"

namespace dalia {

    struct StreamingContext;
    struct Soundbank;

    struct IoRequest {
        enum class Type {
            None,
            RefillStreamBuffer,
            ReleaseStream,
            LoadSoundbank
        };
        Type type = Type::None;

        union {
            struct {
                uint32_t poolIndex;
                uint32_t  generation;
                uint32_t bufferIndex;
            } stream;

            struct {
                Soundbank* targetBank; // TODO: This should not be pointer (Change to an index in a soundbank table)
                uint32_t assetID; // Maybe this should be a filepath? (if not we need a lookup table)
            } bank;
        } data = {};

        static IoRequest RefillStreamBuffer(uint32_t poolIndex, uint32_t generation, uint32_t bufferIndex) {
            IoRequest req;
            req.type = Type::RefillStreamBuffer;
            req.data.stream.poolIndex = poolIndex;
            req.data.stream.generation = generation;
            req.data.stream.bufferIndex = bufferIndex;
            return req;
        }

        static IoRequest ReleaseStream(uint32_t poolIndex) {
            IoRequest req;
            req.type = Type::ReleaseStream;
            req.data.stream.poolIndex = poolIndex;
            return req;
        }

        static IoRequest LoadBank(Soundbank* targetBank, const uint32_t assetID) {
            IoRequest req;
            req.type = Type::LoadSoundbank;
            req.data.bank.targetBank = targetBank;
            req.data.bank.assetID = assetID;
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