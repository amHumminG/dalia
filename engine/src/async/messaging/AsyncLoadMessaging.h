#pragma once
#include "dalia/Result.h"
#include "core/SPSCRingBuffer.h"
#include "mixer/Voice.h"
#include "io/codec/Codec.h"

#include <cstdint>

namespace dalia {

	struct AsyncLoadCommand {
		enum class Type {
			None,

			LoadSound,
			// LoadBank,


			Count // Determines the number of types
		};

		Type type = Type::None;
		uint32_t requestId = INVALID_REQUEST_ID;
		uint64_t assetRawId = INVALID_RAW_ID;
		std::vector<float>* targetPcmBuffer = nullptr;
		char filepath[MAX_STR_LEN_ASSET_PATH]{};

		static AsyncLoadCommand LoadSound(uint32_t reqId,  uint64_t rawId, const char* filepath, std::vector<float>* pcmBuffer = nullptr) {
			AsyncLoadCommand cmd{};
			cmd.type = Type::LoadSound;
			cmd.requestId = reqId;
			cmd.assetRawId = rawId;
			cmd.targetPcmBuffer = pcmBuffer;
			snprintf(cmd.filepath, MAX_STR_LEN_ASSET_PATH, "%s", filepath);
			return cmd;
		}
	};

    struct AsyncLoadEvent {
        enum class Type {
            None,

            SoundLoaded,
            SoundLoadFailed,


        	Count // Determines the number of types
        };

    	Type type = Type::None;
        uint32_t requestId = INVALID_REQUEST_ID;
    	uint64_t assetRawId = INVALID_RAW_ID;
        Result result = Result::Ok;
    	AudioFormatInfo formatInfo{};

        static AsyncLoadEvent SoundLoaded(uint32_t reqId, uint64_t rawId, const AudioFormatInfo& info) {
            AsyncLoadEvent ev{};
            ev.type = Type::SoundLoaded;
            ev.requestId = reqId;
            ev.assetRawId = rawId;
        	ev.result = Result::Ok;
        	ev.formatInfo = info;
            return ev;
        }

        static AsyncLoadEvent SoundLoadFailed(uint32_t reqId, uint64_t rawId, Result errorResult) {
            AsyncLoadEvent ev;
            ev.type = Type::SoundLoadFailed;
            ev.requestId = reqId;
        	ev.assetRawId = rawId;
            ev.result = errorResult;
            return ev;
        }
    };

	// --- Queues ---

	using AsyncLoadCommandQueue = SPSCRingBuffer<AsyncLoadCommand>;
	using AsyncLoadEventQueue = SPSCRingBuffer<AsyncLoadEvent>;

}