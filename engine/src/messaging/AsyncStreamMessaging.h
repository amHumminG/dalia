#pragma once

#include "dalia/SoundControl.h"
#include "core/MPSCRingBuffer.h"

namespace dalia {

    struct AsyncStreamRequest {
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
                char filepath[MAX_STR_LEN_ASSET_PATH];
            } streamPrep;

            struct {
                uint32_t bufferIndex;
            } streamRefill;

            struct {
                uint32_t seekFrame;
            } streamSeek;
        } data = {};

        static AsyncStreamRequest PrepareStream(uint32_t index, uint32_t gen, const char* path) {
            AsyncStreamRequest req;
            req.type = Type::PrepareStream;
            req.index = index;
        	req.gen = gen;
        	snprintf(req.data.streamPrep.filepath, MAX_STR_LEN_ASSET_PATH, "%s", path);
            return req;
        }

        static AsyncStreamRequest ReleaseStream(uint32_t index, uint32_t gen) {
            AsyncStreamRequest req;
            req.type = Type::ReleaseStream;
            req.index = index;
        	req.gen = gen;
            return req;
        }

        static AsyncStreamRequest RefillStreamBuffer(uint32_t index, uint32_t gen, uint32_t bufferIndex) {
            AsyncStreamRequest req;
            req.type = Type::RefillStreamBuffer;
            req.index = index;
            req.gen = gen;
            req.data.streamRefill.bufferIndex = bufferIndex;
            return req;
        }

        static AsyncStreamRequest SeekStream(uint32_t index, uint32_t gen, uint32_t seekFrame) {
            AsyncStreamRequest req{};
            req.type = Type::SeekStream;
            req.index = index;
            req.gen = gen;
            req.data.streamSeek.seekFrame = seekFrame;
            return req;
        }
    };

	// --- Queues ---

	using AsyncStreamRequestQueue = MPSCRingBuffer<AsyncStreamRequest>;
}
