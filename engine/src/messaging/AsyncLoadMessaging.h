#pragma once
#include "dalia/core/Result.h"
#include "core/SPSCRingBuffer.h"
#include "mixer/Voice.h"
#include <cstdint>

namespace dalia {

	struct AsyncLoadRequest {
		enum class Type {
			None,

			LoadSound,
			// LoadBank

		} type = Type::None;

		uint32_t requestId;

		union {
			struct {
				uint64_t resourceHandleRawId;
				char filepath[MAX_STR_LEN_ASSET_PATH];
			} soundFromFile;

		} data = {};

		static AsyncLoadRequest LoadSound(uint32_t reqId, SoundHandle handle, const char* filepath) {
			AsyncLoadRequest req;
			req.type = Type::LoadSound;
			req.requestId = reqId;

			req.data.soundFromFile.resourceHandleRawId = handle.GetRawId();
			snprintf(req.data.soundFromFile.filepath, MAX_STR_LEN_ASSET_PATH, "%s", filepath);

			return req;
		}
	};

    struct AsyncLoadEvent {
        enum class Type {
            None,

            SoundLoaded,
            SoundLoadFailed,
        } type = Type::None;

        uint32_t requestId = 0;
        Result result = Result::Ok;

        uint64_t assetRawId = 0;

        static AsyncLoadEvent SoundLoaded(uint32_t reqId, uint64_t assetRawId) {
            AsyncLoadEvent ev;
            ev.type = Type::SoundLoaded;
            ev.requestId = reqId;
            ev.assetRawId = assetRawId;
            return ev;
        }

        static AsyncLoadEvent SoundLoadFailed(uint32_t reqId, Result error) {
            AsyncLoadEvent ev;
            ev.type = Type::SoundLoadFailed;
            ev.requestId = reqId;
            ev.result = error;
            return ev;
        }
    };

	// --- Queues ---

	using AsyncLoadRequestQueue = SPSCRingBuffer<AsyncLoadRequest>;
	using AsyncLoadEventQueue = SPSCRingBuffer<AsyncLoadEvent>;

}