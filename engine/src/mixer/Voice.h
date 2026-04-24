#pragma once

#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "core/Constants.h"
#include "core/Math.h"
#include "mixer/Resampler.h"

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
		float pitch = PITCH_DEFAULT;
		float stereoPan = PAN_STEREO_DEFAULT;

		bool isLooping = false;
		bool isSpatial = false;

		// Only used if isSpatial is true
		DistanceMode distanceMode = DistanceMode::FromListener;
		AttenuationCurve attenuationModel = AttenuationCurve::InverseSquare;
		math::Vector3 position{0.0f, 0.0f, 0.0f};
		float minDistance = MIN_DIST_DEFAULT;
		float maxDistance = MAX_DIST_DEFAULT;

		bool useDoppler = false;
		float dopplerFactor = 1.0f;
		math::Vector3 velocity{0.0f, 0.0f, 0.0f};

		ListenerMask listenerMask = MASK_ALL_LISTENERS;
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
    	ResamplerState resamplerState;

    	float currentFadeGain = GAIN_DEFAULT; // Owned by the voice (used for micro-fading)
    	float targetFadeGain = GAIN_DEFAULT;  // Owned by the resolver (used for micro-fading)

    	float currentGainMatrix[CHANNELS_MAX][CHANNELS_MAX];
    	float targetGainMatrix[CHANNELS_MAX][CHANNELS_MAX];

    	float currentPitch = 1.0f; // Final resolved pitch after doppler has been applied

    	VoiceParams params;
    	bool isParamsDirty = false;

        bool isLooping = false;

        uint32_t channels = 0;
        uint32_t sampleRate = 0;
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

        	resamplerState = ResamplerState{};

        	currentFadeGain = GAIN_DEFAULT;
        	targetFadeGain = GAIN_DEFAULT;

        	for (uint32_t inC = 0; inC < CHANNELS_MAX; inC++) {
        		for (uint32_t outC = 0; outC < CHANNELS_MAX; outC++) {
        			currentGainMatrix[inC][outC] = GAIN_SILENCE;
        			targetGainMatrix[inC][outC]  = GAIN_SILENCE;
        		}
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
        PlaybackExitCallback onStopCallback = nullptr;
        uint64_t assetRawId;

        // --- Voice Lifecycle ---
        uint32_t gen = START_GENERATION;
        VoiceState state = VoiceState::Free;

        // Routing
        uint32_t parentBusIndex = MASTER_BUS_INDEX;

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
            assetRawId = 0;

            gen++;
            if (gen == NO_GENERATION) gen = START_GENERATION;
            state = VoiceState::Free;

        	parentBusIndex = MASTER_BUS_INDEX;

        	params = VoiceParams{};
        	isParamsDirty = false;

        	frameCount = 0;
        	channels = 0;
        	sampleRate = 0;
            soundType = SoundType::None;
        }
    };
}
