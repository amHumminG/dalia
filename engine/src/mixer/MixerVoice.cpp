#include "async/messaging/AsyncMessageBus.h"
#include "mixer/Mixer.h"
#include "mixer/nodes/Voice.h"
#include "mixer/nodes/StreamContext.h"
#include "mixer/nodes/Listener.h"

#include "mixer/dsp/Spatial.h"
#include "mixer/dsp/Buffer.h"

namespace dalia {

	struct SourceBlock {
		const float* pcmData = nullptr;
		uint32_t framesAvailable = 0;
		uint32_t channels = 0;
		bool isError = false;
		bool isUnderrun = false;
	};

	inline SourceBlock FetchSourceBlock(Voice& voice, StreamContext* streamPool, uint32_t maxFramesNeeded) {
		SourceBlock block;
		uint32_t cursorInt = static_cast<uint32_t>(voice.cursor);

		if (voice.soundType == SoundType::Resident) {

			block.channels = voice.channels;
			uint32_t totalSourceFrames = voice.data.resident.frameCount;

			if (cursorInt < totalSourceFrames) {
				block.framesAvailable = std::min(maxFramesNeeded, totalSourceFrames - cursorInt);
				block.pcmData = voice.data.resident.pcmData + (cursorInt * block.channels);
			}

			return block;
		}
		else if (voice.soundType == SoundType::Stream) {
			StreamContext& stream = streamPool[voice.data.stream.streamContextIndex];
			StreamState streamState = stream.state.load(std::memory_order_acquire);

			if (streamState == StreamState::Error) {
				block.isError = true;
				return block;
			}

			if (!stream.bufferReady[voice.data.stream.frontBufferIndex].load(std::memory_order_acquire)) {
				block.isUnderrun = true;
				return block;
			}

			block.channels = stream.channels;
			uint32_t framesInSource = STREAM_BUFFER_FRAMES;
			const uint32_t eofIndex = stream.eofIndex[voice.data.stream.frontBufferIndex];

			if (!voice.params.isLooping && eofIndex != NO_EOF) framesInSource = eofIndex;

			if (cursorInt < framesInSource) {
				block.framesAvailable = std::min(maxFramesNeeded, framesInSource - cursorInt);
				block.pcmData = stream.buffers[voice.data.stream.frontBufferIndex] + (cursorInt * block.channels);
			}

			return block;
		}

		block.isError = true;
		return block;
	}

	inline bool HandleVoiceBoundary(Voice& voice, StreamContext* streamPool, AsyncMessageBus* asyncMessageBus) {
		if (voice.soundType == SoundType::Resident) {
			if (voice.params.isLooping) {
				voice.cursor -= static_cast<double>(voice.data.resident.frameCount);
				if (voice.cursor < 0.0) voice.cursor = 0.0;
				return true;
			}
			else {
				voice.currentState = VoiceState::Stopped;
				voice.exitCondition = PlaybackExitCondition::NaturalEnd;
				return false;
			}
		}
		else if (voice.soundType == SoundType::Stream) {
			StreamContext& stream = streamPool[voice.data.stream.streamContextIndex];

			// Exit check
			if (!voice.params.isLooping && stream.eofIndex[voice.data.stream.frontBufferIndex] != NO_EOF) {
				voice.currentState = VoiceState::Stopped;
				voice.exitCondition = PlaybackExitCondition::NaturalEnd;
				return false;
			}

			// No exit -> Swap front buffer
			stream.bufferReady[voice.data.stream.frontBufferIndex].store(false, std::memory_order_release);
			asyncMessageBus->PushStreamCommand(AsyncStreamCommand::RefillStreamBuffer(
				voice.data.stream.streamContextIndex,
				stream.gen.load(std::memory_order_relaxed),
				voice.data.stream.frontBufferIndex
			));
			voice.data.stream.frontBufferIndex = 1 - voice.data.stream.frontBufferIndex;
			voice.cursor -= static_cast<double>(STREAM_BUFFER_FRAMES);
			if (voice.cursor < 0.0) voice.cursor = 0.0;

			return true;
		}

		return false;
	}

	void Mixer::HandleAllocateVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];

		voice.gen = cmd.targetGen;
		voice.currentState = VoiceState::Inactive;
		voice.targetState = VoiceState::Inactive;
	}

	void Mixer::HandleFreeVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen) return;

		voice.Reset();
	}

	void Mixer::HandlePrepareVoiceResidentCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		// Voice setup
		voice.soundType = SoundType::Resident;

		voice.data.resident.pcmData = cmd.data.prepResident.pcmData;
		voice.data.resident.frameCount = cmd.data.prepResident.totalFrames;

		voice.channels = cmd.data.prepResident.channels;
		voice.sampleRate = cmd.data.prepResident.sampleRate;
	}

	void Mixer::HandlePrepareVoiceStreamingCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		// Voice setup
		voice.soundType = SoundType::Stream;

		voice.data.stream.streamContextIndex = cmd.data.prepStreaming.streamIndex;
		voice.data.stream.streamContextGen = cmd.data.prepStreaming.streamGen;
		voice.data.stream.frontBufferIndex = 0;

		voice.channels = cmd.data.prepStreaming.channels;
		voice.sampleRate = cmd.data.prepStreaming.sampleRate;
	}

	void Mixer::HandleSeekVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.hasPendingSeek = true;
		voice.pendingSeekFrame = cmd.data.seek.seekFrame;
	}

	void Mixer::HandlePlayVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetState = VoiceState::Playing;
	}

	void Mixer::HandlePauseVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetState = VoiceState::Paused;
	}

	void Mixer::HandleStopVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetState = VoiceState::Stopped;
		voice.isExiting = true;
	}

	void Mixer::HandleSetVoiceParentCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetBusIndex = cmd.data.setParent.parentIndex;
	}

	void Mixer::ResolveVoiceStates() {
		for (uint32_t vIndex = 0; vIndex < m_voicePool.capacity; vIndex++) {
			Voice& voice = m_voicePool.nodes[vIndex];

			// Skip unused voices
			if (voice.currentState == VoiceState::Free) continue;

			// Garbage collection
			if (voice.currentState == VoiceState::Stopped || voice.currentBusIndex == NO_PARENT) {
				if (voice.currentBusIndex == NO_PARENT && voice.currentState != VoiceState::Stopped) {
					// Voice was orphaned
					voice.exitCondition = PlaybackExitCondition::ExplicitStop;
				}
				FreeVoice(vIndex);
				continue;
			}

			// State transitions
			bool requiresSilence = false;

			if (voice.currentState != voice.targetState && voice.targetState != VoiceState::Playing) requiresSilence = true;
			if (voice.currentBusIndex != voice.targetBusIndex) requiresSilence = true;
			if (voice.hasPendingSeek) requiresSilence = true;

			// Do we need to duck?
			if (requiresSilence) voice.targetFadeGain = 0.0f;

			if (math::NearlyEqual(voice.currentFadeGain, 0.0f, EPSILON_GAIN) || voice.currentState != VoiceState::Playing) {
				// Voice is silent

				// Handle Routing swap
				if (voice.currentBusIndex != voice.targetBusIndex) voice.currentBusIndex = voice.targetBusIndex;

				// Handle seek command
				if (voice.hasPendingSeek) {
					if (voice.soundType == SoundType::Resident) {
						voice.cursor = static_cast<double>(voice.pendingSeekFrame);
						voice.hasPendingSeek = false;
						voice.pendingSeekFrame = 0;
					}
					else if (voice.soundType == SoundType::Stream) {
						StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
						StreamState expectedState = StreamState::Streaming;
						if (stream.state.compare_exchange_strong(expectedState, StreamState::Seeking,
							std::memory_order_release)) {
							// If streaming we can push the command to seek the stream

							m_asyncMessageBus->PushStreamCommand(AsyncStreamCommand::SeekStream(
								voice.data.stream.streamContextIndex,stream.gen, voice.pendingSeekFrame
							));

							voice.cursor = 0.0;
							voice.data.stream.frontBufferIndex = 0;
							voice.hasPendingSeek = false;
							voice.pendingSeekFrame = 0;
						}
					}
				}

				// Check if we are waiting for and I/O operation
				if (voice.soundType == SoundType::Stream) {
					StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
					if (stream.state.load(std::memory_order_acquire) != StreamState::Streaming) continue;
				}

				// Handle state change
				if (voice.currentState != voice.targetState) {
					if (voice.targetState == VoiceState::Stopped) {
						voice.exitCondition = PlaybackExitCondition::ExplicitStop;
					}
					voice.currentState = voice.targetState;
				}
			}

			if (!requiresSilence && voice.currentState == VoiceState::Playing) voice.targetFadeGain = 1.0f;
		}
	}

	void Mixer::ResolveVoiceAcoustics() {
		for (uint32_t vIndex = 0; vIndex < m_voicePool.capacity; vIndex++) {
			Voice& voice = m_voicePool.nodes[vIndex];
			if (voice.currentState != VoiceState::Playing) continue;

			float baseGain = voice.params.gain;

			for (uint32_t inC = 0; inC < CHANNELS_MAX; inC++) {
				for (uint32_t outC = 0; outC < CHANNELS_MAX; outC++) {
					voice.targetGainMatrix[inC][outC] = GAIN_SILENCE;
				}
			}

			voice.finalPlaybackRate = voice.params.playbackRate;

			if (!voice.params.isSpatial) {
				// --- NOT SPATIAL ---
				if (voice.channels == CHANNELS_MONO) {
					// Constant power panning
					float panNormalized = (voice.params.stereoPan + 1.0f) * 0.5f;
					float leftSqr = 1.0f - panNormalized;
					float rightSqr = panNormalized;

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

				continue;
			}

			// --- SPATIAL ---
			float maxSpatialGain = GAIN_SILENCE;
			ListenerParams bestListenerParams;

			for (uint32_t lIndex = 0; lIndex < m_listenerPool.capacity; lIndex++) {
				Listener& listener = m_listenerPool.nodes[lIndex];
				if (!listener.params.isActive) continue; // We could evaluate elsewhere for better branch prediction?
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
				if (distGain > maxSpatialGain) {
					maxSpatialGain = distGain;
					bestListenerParams = listener.params;
				}
			}

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
			float totalDopplerFactor = voice.params.dopplerFactor * m_globalDopplerFactor;
			if (voice.params.useDoppler && totalDopplerFactor > 0.0f) {
				math::Vector3 listenerToEmitter = voice.params.position - bestListenerParams.position;
				float distance = math::Vector3::Length(listenerToEmitter);

				if (distance > EPSILON) {
					math::Vector3 direction = listenerToEmitter * (1.0f / distance);

					// Radial velocities
					float listenerRadial = bestListenerParams.velocity.Dot(direction);
					float emitterRadial = voice.params.velocity.Dot(direction);

					// Scale velocities to avoid powf call
					listenerRadial *= totalDopplerFactor;
					emitterRadial *= totalDopplerFactor;

					listenerRadial = std::max(listenerRadial, -SPEED_OF_SOUND);
					emitterRadial = std::max(emitterRadial, -SPEED_OF_SOUND + EPSILON);

					float dopplerMultiplier = (SPEED_OF_SOUND + listenerRadial) / (SPEED_OF_SOUND + emitterRadial);

					voice.finalPlaybackRate = std::clamp(voice.finalPlaybackRate * dopplerMultiplier, PLAYBACK_RATE_MIN, PLAYBACK_RATE_MAX);
				}
			}

			// Implicit downmix
			// As we are pushing the inputs of multiple channels into the same speaker we have to make sure the volume
			// stays the same
			float channelAttenuation = 1.0f / static_cast<float>(voice.channels);
			finalGain *= channelAttenuation;

			for (uint32_t inC = 0; inC < voice.channels; inC++) {
				for (uint32_t outC = 0; outC < m_outChannels; outC++) {
					voice.targetGainMatrix[inC][outC] = finalGain * vbapDistribution[outC];
				}
			}
		}
	}

	uint32_t Mixer::RenderVoices(uint32_t frameCount) {
		uint32_t voicesRendered = 0;

		for (uint32_t vIndex = 0; vIndex < m_voicePool.capacity; vIndex++) {
			Voice& voice = m_voicePool.nodes[vIndex];

			// Skip unused voices
			if (voice.currentState != VoiceState::Playing) continue;

			voicesRendered++;
			float* busBuffer = GetBusBuffer(voice.currentBusIndex);
			uint32_t framesMixed = 0;

			float phaseInc = voice.finalPlaybackRate * (static_cast<float>(voice.sampleRate) / static_cast<float>(m_outSampleRate));

			while (framesMixed < frameCount) {
				uint32_t outputFramesNeeded = frameCount - framesMixed;
				uint32_t sourceFramesNeeded = static_cast<uint32_t>(
					std::ceil(static_cast<float>(outputFramesNeeded) * phaseInc)) + HERMITE_LOOKAHEAD_FRAMES;

				SourceBlock block = FetchSourceBlock(voice, m_streamPool.data(), sourceFramesNeeded);
				if (block.isError) {
					voice.currentState = VoiceState::Stopped;
					voice.exitCondition = PlaybackExitCondition::Error;
					break;
				}
				if (block.isUnderrun) {
					DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Stream buffer underrun for voice %u.", vIndex);
					break;
				}

				// Render block
				if (block.framesAvailable > 0) {
					uint32_t outputFramesGenerated = 0;
					uint32_t sourceFramesConsumed = 0;

					ProcessResampler(
						voice.resamplerState,
						block.pcmData,
						block.framesAvailable,
						block.channels,
						m_mixScratchBuffer.get(),
						outputFramesNeeded,
						phaseInc,
						outputFramesGenerated,
						sourceFramesConsumed
					);

					dsp::MixVoiceBlock(
						m_mixScratchBuffer.get(),
						&busBuffer[framesMixed * m_outChannels],
						outputFramesGenerated,
						block.channels,
						m_outChannels,
						voice.currentFadeGain,
						voice.targetFadeGain,
						m_fadeStep,
						voice.currentGainMatrix,
						voice.targetGainMatrix,
						m_smoothingCoefficient
					);

					voice.cursor += static_cast<double>(sourceFramesConsumed);
					framesMixed += outputFramesGenerated;
				}

				// Boundary check
				if (block.framesAvailable < sourceFramesNeeded) {
					bool keepPlaying = HandleVoiceBoundary(voice, m_streamPool.data(), m_asyncMessageBus);
					if (!keepPlaying) break;
				}
			}
		}

		return voicesRendered;
	}

	void Mixer::FreeVoice(uint32_t voiceIndex) {
		Voice& voice = m_voicePool.nodes[voiceIndex];

		if (voice.soundType == SoundType::Stream) {
			m_asyncMessageBus->PushStreamCommand(AsyncStreamCommand::ReleaseStream(
				voice.data.stream.streamContextIndex, m_streamPool[voice.data.stream.streamContextIndex].gen
			));
		}

		if (voice.exitCondition == PlaybackExitCondition::NaturalEnd) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d finished naturally.", voiceIndex);
		}
		else if (voice.exitCondition == PlaybackExitCondition::ExplicitStop) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d stopped explicitly.", voiceIndex);
		}
		else if (voice.exitCondition == PlaybackExitCondition::Evicted) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d evicted.", voiceIndex);
		}
		else if (voice.exitCondition == PlaybackExitCondition::Error) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d stopped by error.", voiceIndex);
		}

		m_asyncMessageBus->PushMixerEvent(AsyncMixerEvent::VoiceStopped(
			voiceIndex, voice.gen, voice.exitCondition
		));
		voice.Reset();
	}
}
