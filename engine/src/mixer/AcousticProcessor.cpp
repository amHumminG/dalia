#include "mixer/AcousticProcessor.h"
#include "mixer/dsp/Spatial.h"
#include "async/messaging/AsyncMessageBus.h"
#include "async/messaging/AsyncMixerMessaging.h"
#include "core/Math.h"

#include <algorithm>

namespace dalia {

	AcousticProcessor::~AcousticProcessor() {
		Shutdown();
	}

	void AcousticProcessor::Initialize(AsyncMessageBus* asyncMessageBus, const AcousticProcessorConfig& config) {
		m_asyncMessageBus  = asyncMessageBus;
		m_coordinateSystem = config.coordinateSystem;
		m_listenerPool     = config.listenerPool;

		SetOutputFormat(config.outChannels, config.speakerLayout);

		SubscribeToCommands();
	}

	void AcousticProcessor::Shutdown() {
		if (!m_asyncMessageBus) return;

		UnsubscribeFromCommands();
		m_asyncMessageBus = nullptr;
	}

	void AcousticProcessor::SetOutputFormat(uint32_t outChannels, SpeakerLayout layout) {
		m_outChannels = outChannels;
		m_speakerLayout = layout;
		ConfigureSpeakerLayout(layout);
	}

	void AcousticProcessor::EvaluateVoices(std::span<Voice> voices) {
		for (Voice& voice : voices) {
			if (voice.currentState != VoiceState::Playing) continue;

			for (uint32_t inC = 0; inC < CHANNELS_MAX; inC++) {
				for (uint32_t outC = 0; outC < CHANNELS_MAX; outC++) {
					voice.targetGainMatrix[inC][outC] = GAIN_SILENCE;
				}
			}

			voice.finalPlaybackRate = voice.params.playbackRate;

			if (!voice.params.isSpatial) EvaluateNonSpatialVoice(voice);
			else EvaluateSpatialVoice(voice);
		}
	}

	void AcousticProcessor::SubscribeToCommands() {
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::SetGlobalDopplerFactor,
			[this](const AsyncMixerCommand& cmd) { HandleSetGlobalDopplerFactorCommand(cmd); });
	}

	void AcousticProcessor::UnsubscribeFromCommands() {
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SetGlobalDopplerFactor);
	}

	void AcousticProcessor::HandleSetGlobalDopplerFactorCommand(const AsyncMixerCommand& cmd) {
		m_globalDopplerFactor = cmd.data.floatVal.value;
	}

	void AcousticProcessor::ConfigureSpeakerLayout(SpeakerLayout layout) {
		for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
			m_speakerMatrix[c] = { math::Vector3(0, 0, 0), c };
		}

		if (layout == SpeakerLayout::Mono) {
			m_speakerMatrix[0] = { math::AngleToVector3(0.0f),  0 };
			m_spatialSpeakerCount = 1;
		}
		else if (layout == SpeakerLayout::Stereo) {
			m_speakerMatrix[0] = { math::AngleToVector3(-30.0f), 0 }; // Left
			m_speakerMatrix[1] = { math::AngleToVector3(30.0f), 1 };  // Right
			m_spatialSpeakerCount = 2;
		}
		else if (layout == SpeakerLayout::Surround51) {
			// TODO: Implement before supporting 5.1 surround sound
			// m_speakerMatrix[0] = { math::AngleToVector3(-30.0f), 0 }; // Left
			// m_speakerMatrix[1] = { math::AngleToVector3(30.0f), 1 }; // Right
			// m_speakerMatrix[2] = { math::AngleToVector3(0.0f), 2 }; // Center
			// // Channel 3 is LFE (Subwoofer should not be spatialized)
			// m_speakerMatrix[3] = { math::AngleToVector3(-110.0f), 4 }; // Surround Left
			// m_speakerMatrix[4] = { math::AngleToVector3(110.0f), 5 }; // Surround Right
			// m_spatialSpeakerCount = 5;
		}
		else if (layout == SpeakerLayout::Surround71) {
			// TODO: Implement before supporting 7.1 surround sound
		}
	}

	void AcousticProcessor::EvaluateNonSpatialVoice(Voice& voice) const {
		float baseGain = voice.params.gain;

		if (voice.channels == CHANNELS_MONO) {
			// Constant power panning
			float panNormalized = (voice.params.stereoPan + 1.0f) * 0.5f;
			float leftSqr		= 1.0f - panNormalized;
			float rightSqr		= panNormalized;

			float gainL = (leftSqr > EPSILON_GAIN) ? leftSqr * math::CalculateInvSqrt(leftSqr) : GAIN_SILENCE;
			float gainR = (rightSqr > EPSILON_GAIN) ? rightSqr * math::CalculateInvSqrt(rightSqr) : GAIN_SILENCE;

			voice.targetGainMatrix[0][0] = gainL * baseGain;
			voice.targetGainMatrix[0][1] = gainR * baseGain;
		}
		else if (voice.channels == CHANNELS_STEREO) {
			// Stereo panning
			float gainL = std::clamp(1.0f - voice.params.stereoPan, 0.0f, 1.0f);
			float gainR = std::clamp(1.0f + voice.params.stereoPan, 0.0f, 1.0f);

			voice.targetGainMatrix[0][0] = gainL * baseGain;
			voice.targetGainMatrix[1][1] = gainR * baseGain;
		}
		else {
			uint32_t limit = std::min(voice.channels, m_outChannels);
			for (uint32_t c = 0; c < limit; c++) {
				voice.targetGainMatrix[c][c] = baseGain;
			}
		}
	}

	void AcousticProcessor::EvaluateSpatialVoice(Voice& voice) const {
		ListenerParams bestListenerParams{};
		float maxSpatialGain = GAIN_SILENCE;

		if (!FindBestListener(voice, bestListenerParams, maxSpatialGain)) {
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Failed to find listener for spatial playback instance.");
			return;
		}

		float baseGain = voice.params.gain;
		float finalGain = baseGain * maxSpatialGain;
		float vbapDistribution[CHANNELS_MAX] = {};

		// Panning
		if (finalGain > EPSILON_GAIN) {
			dsp::VBAP(
				m_coordinateSystem,
				voice.params.position,
				bestListenerParams.position,
				bestListenerParams.forward,
				bestListenerParams.up,
				m_speakerMatrix,
				m_spatialSpeakerCount,
				vbapDistribution
			);
		}

		// Final Playback Rate Evaluation
		if (voice.params.useDoppler) {
			float dopplerMult = ComputeDopplerMultiplier(voice, bestListenerParams);
			voice.finalPlaybackRate = std::clamp(
				voice.finalPlaybackRate * dopplerMult,
				PLAYBACK_RATE_MIN,
				PLAYBACK_RATE_MAX
			);
		}

		// Implicit downmix
		// As we are pushing the inputs of multiple channels into the same speaker we have to make sure the volume
		// stays the same
		finalGain /= static_cast<float>(voice.channels);

		for (uint32_t inC = 0; inC < voice.channels; inC++) {
			for (uint32_t outC = 0; outC < m_outChannels; outC++) {
				voice.targetGainMatrix[inC][outC] = finalGain * vbapDistribution[outC];
			}
		}
	}

	bool AcousticProcessor::FindBestListener(const Voice& voice, ListenerParams& outBestListenerParams,
		float& outMaxGain) const {
		outMaxGain = GAIN_SILENCE;
		bool found = false;

		for (uint32_t lIndex = 0; lIndex < m_listenerPool.capacity; lIndex++) {
			Listener& listener = m_listenerPool.nodes[lIndex];
			if (!listener.params.isActive) continue;
			if (!(voice.params.listenerMask & (1 << lIndex))) continue;

			float distance;
			if (voice.params.distanceMode == DistanceMode::FromListener) {
				distance = voice.params.position.DistanceTo(listener.params.position);
			}
			else {
				distance = voice.params.position.DistanceTo(listener.params.distanceProbePosition);
			}

			float distGain = dsp::GetDistanceAttenuationGain(
				distance,
				voice.params.minDistance,
				voice.params.maxDistance,
				voice.params.attenuationCurve
			);

			// NOTE: If we add directional sound cones or occlusion in the future, this is probably where we do that

			// Check if the voice was loudest for this listener
			if (distGain > outMaxGain) {
				outMaxGain = distGain;
				outBestListenerParams = listener.params;
				found = true;
			}
		}

		return found;
	}

	float AcousticProcessor::ComputeDopplerMultiplier(const Voice& voice, const ListenerParams& listenerParams) const {
		float totalDopplerFactor = voice.params.dopplerFactor * m_globalDopplerFactor;
		if (totalDopplerFactor <= 0.0f) return 1.0f;

		math::Vector3 listenerToEmitter = voice.params.position - listenerParams.position;
		float distance = math::Vector3::Length(listenerToEmitter);
		if (distance >= EPSILON) return 1.0f;

		math::Vector3 direction = listenerToEmitter * (1.0f / distance);

		// Radial velocities
		float listenerRadial = listenerParams.velocity.Dot(direction);
		float emitterRadial = voice.params.velocity.Dot(direction);

		// Scale velocities to avoid powf call
		listenerRadial *= totalDopplerFactor;
		emitterRadial *= totalDopplerFactor;

		listenerRadial = std::max(listenerRadial, -SPEED_OF_SOUND);
		emitterRadial = std::max(emitterRadial, -SPEED_OF_SOUND + EPSILON);

		return (SPEED_OF_SOUND + listenerRadial) / (SPEED_OF_SOUND + emitterRadial);
	}
}
