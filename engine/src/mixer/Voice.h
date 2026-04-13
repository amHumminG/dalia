#pragma once

#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "core/Constants.h"
#include "core/Math.h"
#include "StreamContext.h"

#include <cstdint>

namespace dalia {

    enum class VoiceState : uint8_t {
        Free,
        Inactive,
        Playing,
        Paused,
        Stopped
    };

	struct VoiceParams {
		float volumeDb = VOLUME_DB_DEFAULT;
		float pitch = 1.0f;
		float stereoPan = PAN_STEREO_DEFAULT;

		bool isLooping = false;
		bool isSpatial = false;

		// Only used if isSpatial is true
		AttenuationModel attenuationModel = AttenuationModel::InverseSquare;
		math::Vector3 position{0.0f, 0.0f, 0.0f};
		float minDistance = MIN_DIST_DEFAULT;
		float maxDistance = MAX_DIST_DEFAULT;
	};

    struct Voice {
        uint32_t gen = START_GENERATION;

        VoiceState currentState = VoiceState::Free;
    	VoiceState targetState = VoiceState::Free;

        PlaybackExitCondition exitCondition = PlaybackExitCondition::NaturalEnd;
    	bool isExiting = false;

        // Routing
        uint32_t currentBusIndex = MASTER_BUS_INDEX;
    	uint32_t targetBusIndex = MASTER_BUS_INDEX;

    	// Seeking
    	bool hasPendingSeek = false;
    	uint32_t pendingSeekFrame = 0;

        // Mixing Properties
    	float currentFadeGain = GAIN_DEFAULT; // Owned by the voice
    	float targetFadeGain = GAIN_DEFAULT;  // Owned by the resolver

    	float currentGains[CHANNELS_MAX] = {
    		GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT,
    		GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT
    	};
    	float targetGains[CHANNELS_MAX] {
    		GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT,
			GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT, GAIN_DEFAULT
    	};

    	VoiceParams params;
    	bool isParamsDirty = false;

        bool isLooping = false;

        uint32_t channels = CHANNELS_STEREO;
        uint32_t sampleRate = TARGET_OUTPUT_SAMPLE_RATE;
        double cursor = 0.0;

        SoundType soundType = SoundType::None;
        union {
            struct {
                const float* pcmData;
                uint32_t frameCount;
            } resident;

            struct {
                uint32_t streamContextIndex;
                uint32_t frontBufferIndex;
            } stream;
        } data = {};

        // Use this when releasing a voice
        void Reset() {
            gen++;
            if (gen == NO_GENERATION) gen = START_GENERATION;

        	currentState = VoiceState::Free;
        	targetState = VoiceState::Free;

        	exitCondition = PlaybackExitCondition::NaturalEnd;
        	isExiting = false;

            currentBusIndex = MASTER_BUS_INDEX;
        	targetBusIndex = MASTER_BUS_INDEX;

        	hasPendingSeek = false;
			pendingSeekFrame = 0;

        	currentFadeGain = GAIN_DEFAULT;
        	targetFadeGain = GAIN_DEFAULT;

        	for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
        		currentGains[c] = GAIN_DEFAULT;
        		targetGains[c]  = GAIN_DEFAULT;
        	}

            isLooping = false;

        	params = VoiceParams{};
        	isParamsDirty = false;

            channels = 0;
            sampleRate = 0;
            cursor = 0.0;

            soundType = SoundType::None;
            data = {};
        }
    };

    struct VoiceMirror {
        // --- API Thread Only Stuff ---
        bool pendingLoad = false; // Pending playback due to asset loading
        AudioEventCallback onStopCallback = nullptr;
        uint64_t assetUuid;

        // --- Voice Lifecycle ---
        uint32_t gen = START_GENERATION;
        VoiceState state = VoiceState::Free;

        // Routing
        uint32_t parentBusIndex = MASTER_BUS_INDEX;
        bool isLooping = false;

    	VoiceParams params;
    	bool isParamsDirty = false;

        // Asset
        uint32_t frameCount = 0;
        uint32_t channels = 0;
        uint32_t sampleRate = 0;
        SoundType soundType;

        void Reset() {
            pendingLoad = false;
            onStopCallback = nullptr;
            assetUuid = 0;

            gen++;
            if (gen == NO_GENERATION) gen = START_GENERATION;
            state = VoiceState::Free;

            parentBusIndex = MASTER_BUS_INDEX;

            isLooping = false;

            channels = 0;
            soundType = SoundType::None;
        }
    };
}
