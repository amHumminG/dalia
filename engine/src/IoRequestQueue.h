#pragma once
#include "MPSCRingBuffer.h"

namespace dalia {

    struct StreamingContext;
    struct Soundbank;

    struct IoRequest {
        enum class Type {
            None,
            RefillStreamBuffer,
            LoadSoundbank
        };
        Type type = Type::None;

        union {
            struct {
                StreamingContext* context;
                uint8_t bufferIndex;
            } stream;

            struct {
                Soundbank* targetBank;
                uint32_t assetID; // Maybe this should be a filepath? (if not we need a lookup table)
            } bank;
        } data = {};

        static IoRequest RefillStreamBuffer(StreamingContext* stream, uint8_t bufferIndex) {
            IoRequest req;
            req.type = Type::RefillStreamBuffer;
            req.data.stream.context = stream;
            req.data.stream.bufferIndex = bufferIndex;
            return req;
        }

        static IoRequest LoadBank(Soundbank* targetBank, uint32_t assetID) {
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