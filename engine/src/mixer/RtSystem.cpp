#include "RtSystem.h"

#include "core/Logger.h"
#include "core/Constants.h"
#include "core/Math.h"

#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"
#include "mixer/Listener.h"

#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoStreamRequestQueue.h"

#include "dalia/audio/SoundControl.h"

#include <cmath>

#include "MixGraphCompiler.h"
#include "effects/BiquadFilter.h"


namespace dalia {

	// --- Helpers ---

	static inline float CalculateLinearFadeStep(float fadeTimeSeconds, uint32_t sampleRate) {
		return 1.0f / (fadeTimeSeconds * static_cast<float>(sampleRate));
	}

	static inline void MixToBuffer(float* DALIA_RESTRICT targetBuffer, const float* DALIA_RESTRICT buffer,
		uint32_t sampleCount) {
		for (uint32_t i = 0; i < sampleCount; i++) {
			targetBuffer[i] += buffer[i];
		}
	}

	static inline float* GetBusBuffer(float* busBufferPool, uint32_t busIndex, uint32_t frameCount) {
		return busBufferPool + (busIndex * frameCount * CHANNELS_MAX);
	}

	static inline void StepMatrixGains(
		float (* DALIA_RESTRICT current)[CHANNELS_MAX],
		const float (* DALIA_RESTRICT target)[CHANNELS_MAX],
		uint32_t inChannels,
		uint32_t outChannels,
		float smoothingCoefficient) {
		for (uint32_t inC = 0; inC < inChannels; inC++) {
			for (uint32_t outC = 0; outC < outChannels; outC++) {
				float diff = target[inC][outC] - current[inC][outC];

				if (std::abs(diff) < EPSILON_GAIN) current[inC][outC] = target[inC][outC];
				else current[inC][outC] += diff * smoothingCoefficient;
			}
		}
 	}

	static inline void StepFadeGain(float& currentGain, float targetGain, float stepSize) {
		if (currentGain == targetGain) return;

		if (currentGain > targetGain) {
			// Fading down
			currentGain -= stepSize;
			if (currentGain < targetGain) currentGain = targetGain;
		}
		else {
			// Fading up
			currentGain += stepSize;
			if (currentGain > targetGain) currentGain = targetGain;
		}
	}

	static inline void ApplyGainAndFade(float* DALIA_RESTRICT buffer, uint32_t frameCount, uint32_t channels,
		float& currentGain, float targetGain, float smoothingCoefficient,
		float& currentFade, float targetFade, float fadeStep) {

		bool gainStable = math::NearlyEqual(currentGain, targetGain, EPSILON_GAIN);
		bool fadeStable = math::NearlyEqual(currentFade, targetFade, EPSILON_GAIN);

		// Fast path
		if (gainStable && fadeStable) {
			currentGain = targetGain;
			currentFade = targetFade;

			float combinedGain = currentGain * currentFade;

			if (math::NearlyEqual(combinedGain, 1.0f, EPSILON_GAIN)) return;

			uint32_t sampleCount = frameCount * channels;
			if (math::NearlyEqual(combinedGain, 0.0f, EPSILON_GAIN)) {
				std::memset(buffer, 0, sampleCount * sizeof(float));
				return;
			}

			for (uint32_t s = 0; s < sampleCount; s++) buffer[s] *= combinedGain;
			return;
		}

		// Slow path
		for (uint32_t f = 0; f < frameCount; f++) {
			if (!gainStable) currentGain += (targetGain - currentGain) * smoothingCoefficient;
			if (!fadeStable) StepFadeGain(currentFade, targetFade, fadeStep);

			float combinedGain = currentGain * currentFade;
			for (uint32_t c = 0; c < channels; c++) buffer[(f * channels) + c] *= combinedGain;
		}
	}

	// Applies a cross-fade between a dry and wet buffer over a block of frames
	static inline void ApplyBlockCrossFade(float* inOutDryBuffer, const float* inWetBuffer, uint32_t frameCount,
		uint32_t channels, float& currentMix, float targetMix, float mixDelta) {
		for (uint32_t frame = 0; frame < frameCount; frame++) {
			// Increment mix coefficient
			if (currentMix < targetMix) {
				currentMix += mixDelta;
				if (currentMix >= targetMix) currentMix = targetMix;
			}
			else if (currentMix > targetMix) {
				currentMix -= mixDelta;
				if (currentMix < targetMix) currentMix = targetMix;
			}

			for (uint32_t channel = 0; channel < channels; channel++) {
				uint32_t index = (frame * channels) + channel;
				float dry = inOutDryBuffer[index];
				float wet = inWetBuffer[index];

				inOutDryBuffer[index] = dry + currentMix * (wet - dry);
			}
		}
	}

	static inline void ApplySoftClipper(float* DALIA_RESTRICT buffer, uint32_t sampleCount) {
		constexpr float THRESHOLD = 0.8f;
		constexpr float CEILING = 1.0f;
		constexpr float HEADROOM = CEILING - THRESHOLD;

		for (uint32_t i = 0; i < sampleCount; i++) {
			float sample = buffer[i];
			float sampleAbs = std::abs(sample);

			if (sampleAbs > THRESHOLD) {
				float sign = (sample > 0.0f) ? 1.0f : -1.0f;
				float over = sampleAbs - THRESHOLD;
				float compressed = THRESHOLD + (over / (1.0f + (over / HEADROOM)));
				buffer[i] = sign * compressed;
			}
		}
	}

	static float GetDistanceAttenuationGain(float distance, float minDistance, float maxDistance, AttenuationModel model) {
		if (maxDistance <= minDistance) {
			if (distance <= minDistance) return 1.0f;
			if (distance > minDistance)  return 0.0f;
		}

		if (distance <= minDistance) return 1.0f;
		if (distance >= maxDistance) return 0.0f;

		float distanceFactor = (distance - minDistance) / (maxDistance - minDistance); // Factor between 0 and 1

		switch (model) {
			case AttenuationModel::InverseSquare: {
				float inverseFactor = minDistance / distance;
				return inverseFactor * (1.0f - distanceFactor);
			}
			case AttenuationModel::Linear: {
				return 1.0f - distanceFactor;
			}
			case AttenuationModel::Exponential: {
				float inverseFactor = 1.0f - distanceFactor;
				return inverseFactor * inverseFactor;
			}
			default: return 0.0f;
		}
	}

	void VBAP(
		math::Vector3 sourcePos,
		math::Vector3 listenerPos,
		math::Vector3 listenerForward,
		math::Vector3 listenerUp,
		const VirtualSpeaker* speakerMatrix,
		uint32_t spatialSpeakerCount,
		float out[CHANNELS_MAX]) {
		// Convert to listener space
		math::Vector3 relative = sourcePos - listenerPos;
		// FIXME: This operation should depend on the coordinate system
		math::Vector3 listenerRight = math::Vector3::Normalize(listenerUp.Cross(listenerForward));

		float x = relative.Dot(listenerRight);
		float y = relative.Dot(listenerForward);

		math::Vector3 listenerSpaceDir(x, y, 0.0f);
		listenerSpaceDir.Normalize();

		for (uint32_t c = 0; c < CHANNELS_MAX; c++) out[c] = 0.0f; // Clear output

		// --- Routing Logic ---

		if (spatialSpeakerCount == 1) {
			// Mono -> Cannot apply VBAP to mono output
			out[0] = 1.0f;
		}
		else if (spatialSpeakerCount == 2) {
			// Stereo (we assume that speaker matrix [0] is left and [1] is right
			float pan = (listenerSpaceDir.x + 1.0f) * 0.5f;
			float leftSqr = 1.0f - pan;
			float rightSqr = pan;

			uint32_t leftChannel = speakerMatrix[0].channelIndex;
			uint32_t rightChannel = speakerMatrix[1].channelIndex;

			out[leftChannel] = leftSqr * math::CalculateInvSqrt(leftSqr);
			out[rightChannel] = rightSqr * math::CalculateInvSqrt(rightSqr);
		}
		else {
			// TODO: Implement VBAP for 5.1 and 7.1 surround
		}
	}

	// ------------

    RtSystem::RtSystem(const RtSystemConfig& config)
        : m_outChannels(config.outChannels),
		m_outSampleRate(config.outSampleRate),
		m_voicePool(config.voicePool),
		m_voiceParamBridges(config.voiceParamBridges),
        m_streamPool(config.streamPool),
        m_busPool(config.busPool),
		m_busBufferPool(config.busBufferPool),
        m_rtCommands(config.rtCommands),
        m_rtEvents(config.rtEvents),
        m_ioStreamRequests(config.ioStreamRequests),
		m_mixGraphCompiler(config.mixGraphCompiler),
		m_mixOrder(config.mixOrder),
		m_dspScratchBuffer(config.dspScratchBuffer),
		m_biquadFilterPool(config.biquadFilterPool),
		m_listenerPool(config.listenerPool),
		m_listenerParamBridges(config.listenerParamBridges) {
		m_smoothingCoefficient = 1.0f - std::exp(-2.0f * PI * SMOOTHING_CUTOFF_HZ / config.outSampleRate);
		m_fadeStep = CalculateLinearFadeStep(FADE_TIME_GAIN, m_outSampleRate);
    }

    void RtSystem::Tick(float* output, uint32_t frameCount) {
        ProcessCommands();			// Process incoming commands from the API thread
		ProcessParams();			// Process continuous parameter updates from the API thread
        Render(output, frameCount); // Render the audio frame
    }

    void RtSystem::ProcessCommands() {
        RtCommand cmd;
        while (m_rtCommands->Pop(cmd)) {
            switch (cmd.type) {
				case RtCommand::Type::AllocateVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];

	            	voice.gen = cmd.targetGen;
	            	voice.currentState = VoiceState::Inactive;
	            	voice.targetState = VoiceState::Inactive;
		            break;
	            }
				case RtCommand::Type::DeallocateVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen) break;

	            	voice.Reset();
	            	break;
				}
	            case RtCommand::Type::PrepareVoiceResident: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

	            	// Voice setup
	            	voice.soundType = SoundType::Resident;

	            	voice.data.resident.pcmData = cmd.data.prepResident.pcmData;
	            	voice.data.resident.frameCount = cmd.data.prepResident.frameCount;

	            	voice.channels = cmd.data.prepResident.channels;
	            	voice.sampleRate = cmd.data.prepResident.sampleRate;
	            	break;
	            }
				case RtCommand::Type::PrepareVoiceStreaming: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

	            	// Voice setup
	            	voice.soundType = SoundType::Stream;

	            	voice.data.stream.streamContextIndex = cmd.data.prepStreaming.streamIndex;
	            	voice.data.stream.frontBufferIndex = 0;
	            	break;
	            }
				case RtCommand::Type::SeekVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

	            	voice.hasPendingSeek = true;
	            	voice.pendingSeekFrame = cmd.data.seek.seekFrame;
	            	break;
	            }
				case RtCommand::Type::PlayVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

	            	voice.targetState = VoiceState::Playing;
	            	break;
				}
                case RtCommand::Type::PauseVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

					voice.targetState = VoiceState::Paused;
	            	break;
                }
                case RtCommand::Type::StopVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

	            	voice.targetState = VoiceState::Stopped;
	            	voice.isExiting = true;
	            	break;
                }
				case RtCommand::Type::SetVoiceParent: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

					voice.targetBusIndex = cmd.data.setParent.parentIndex;
	            	break;
				}
				case RtCommand::Type::SetVoiceLooping: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

	            	voice.isLooping = cmd.data.boolVal.value;
	            	break;
				}
				case RtCommand::Type::AllocateBus: {
		            Bus& bus = m_busPool[cmd.targetIndex];

	            	bus.currentParentIndex = cmd.data.setParent.parentIndex;
					bus.targetParentIndex = cmd.data.setParent.parentIndex;
					bus.isActive = true;

					m_isMixOrderDirty = true;
	            	break;
	            }
				case RtCommand::Type::DeallocateBus: {
	            	uint32_t bIndex = cmd.targetIndex;

	            	m_busPool[bIndex].Reset();

					m_isMixOrderDirty = true;
	            	break;
				}
				case RtCommand::Type::SetBusParent: {
	            	uint32_t bIndex = cmd.targetIndex;
	            	uint32_t bIndexParent = cmd.data.setParent.parentIndex;

	            	m_busPool[bIndex].targetParentIndex = bIndexParent;
		            break;
	            }
				case RtCommand::Type::SetBusGain: {
	            	uint32_t bIndex = cmd.targetIndex;
	            	float newGain = cmd.data.floatVal.value;

	            	m_busPool[bIndex].targetGain = newGain;
	            	break;
				}
				case RtCommand::Type::AllocateBiquad: {
	            	BiquadFilter& biquad = m_biquadFilterPool[cmd.targetIndex];
	            	biquad.generation = cmd.targetGen;
	            	biquad.type = cmd.data.biquad.type;

					biquad.targetFrequency = cmd.data.biquad.config.frequency;
					biquad.currentFrequency = cmd.data.biquad.config.frequency;
	            	biquad.targetResonance = cmd.data.biquad.config.resonance;
	            	biquad.currentResonance = cmd.data.biquad.config.resonance;
	            	CalculateBiquadCoefficients(biquad, static_cast<float>(m_outSampleRate));
	            	break;
				}
				case RtCommand::Type::SetBiquadParams: {
	            	BiquadFilter& biquad = m_biquadFilterPool[cmd.targetIndex];
	            	if (biquad.generation != cmd.targetGen) break;

	            	biquad.targetFrequency = cmd.data.biquad.config.frequency;
	            	biquad.targetResonance = cmd.data.biquad.config.resonance;
	            	break;
				}
				case RtCommand::Type::AttachEffect: {
	            	EffectHandle effect = EffectHandle::Create(
	            		cmd.targetIndex,
	            		cmd.targetGen,
	            		cmd.data.effect.type
	            	);

					AttachEffect(effect, cmd.data.effect.busIndex, cmd.data.effect.effectSlot);
	            	break;
				}
				case RtCommand::Type::FadeDetachEffect: {
	            	EffectHandle effect = EffectHandle::Create(
						cmd.targetIndex,
						cmd.targetGen,
						cmd.data.effect.type
					);

	            	FadeOutEffect(effect, cmd.data.effect.busIndex, cmd.data.effect.effectSlot);
	            	break;
				}
				case RtCommand::Type::ForceDetachEffect: {
	            	EffectHandle effect = EffectHandle::Create(
						cmd.targetIndex,
						cmd.targetGen,
						cmd.data.effect.type
					);

	            	DetachEffect(effect, cmd.data.effect.busIndex, cmd.data.effect.effectSlot);
	            	break;
				}
				case RtCommand::Type::DeallocateEffect: {
	            	uint32_t efIndex = cmd.targetIndex;
	            	uint32_t efGen = cmd.targetGen;
	            	EffectType efType = cmd.data.effect.type;

	            	switch (efType) {
	            		case EffectType::None: break;
	            		case EffectType::Biquad:
	            			if (m_biquadFilterPool[efIndex].generation == efGen) m_biquadFilterPool[efIndex].Reset();
	            			break;
	            		default: break;
	            	}
	            	break;
				}
                default:
            		break;
            }
        }
    }

    void RtSystem::ProcessParams() {
		VoiceParams vParams;
		for (uint32_t vIndex = 0; vIndex < m_voiceParamBridges.size(); vIndex++) {
			if (m_voiceParamBridges[vIndex].ConsumeUpdate(vParams)) {
				m_voicePool[vIndex].params = vParams;
			}
		}

		ListenerParams lParams;
		for (uint32_t lIndex = 0; lIndex < m_listenerPool.size(); lIndex++) {
			if (m_listenerParamBridges[lIndex].ConsumeUpdate(lParams)) {
				m_listenerPool[lIndex].params = lParams;
			}
		}
    }

    void RtSystem::Render(float* output, uint32_t frameCount) {
        const uint32_t sampleCount = frameCount * m_outChannels;

    	// Bus preprocessing
        std::ranges::fill(m_busBufferPool, 0.0f);
		for (uint32_t bIndex = 0; bIndex < m_busPool.size(); bIndex++) {
			Bus& bus = m_busPool[bIndex];
			if (bus.isActive) m_isMixOrderDirty |= ResolveBus(bus);
		}

		if (m_isMixOrderDirty) {
			m_mixOrderSize = m_mixGraphCompiler->Compile( m_busPool, m_mixOrder);
			m_isMixOrderDirty = false;
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Recompiled bus graph.");
		}

		if (m_mixOrderSize == 0) {
			// TODO: Send an RtEvent back to API thread to log cycle detection
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Mix graph cycle detected!"); // TODO: remove this log

			std::memset(output, 0, sampleCount * sizeof(float));
			return;
		}

        // --- Voice Pass ---
    	uint32_t voicesMixed = 0;
        for (uint32_t vIndex = 0; vIndex < m_voicePool.size(); vIndex++) {
            Voice& voice = m_voicePool[vIndex];

        	if (voice.currentState == VoiceState::Free) continue;
        	if (voice.currentState == VoiceState::Stopped) {
        		FreeVoice(vIndex);
        		continue;
        	}
        	if (voice.currentBusIndex == NO_PARENT) {
        		voice.exitCondition = PlaybackExitCondition::ExplicitStop;
        		FreeVoice(vIndex);
        		continue;
        	}

        	ResolveVoice(voice);

        	if (voice.currentState == VoiceState::Playing) {
            	bool isStillPlaying = ProcessVoice(vIndex, frameCount);
            	if (!isStillPlaying) voice.currentState = VoiceState::Stopped;

            	voicesMixed++;
            }
        }

        // --- Bus Pass ---
		std::span activeMixOrder = m_mixOrder.subspan(0, m_mixOrderSize);
        for (uint32_t bIndex : activeMixOrder) {
            ProcessBus(bIndex, frameCount);
        }

        float* masterBuffer = m_busBufferPool.data();
		ApplySoftClipper(masterBuffer, sampleCount);
        std::copy_n(masterBuffer, sampleCount, output);
	}

	void RtSystem::ResolveVoice(Voice& voice) {
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

			// Handle seek request
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
						// If streaming we can push the request

						m_ioStreamRequests->Push(IoStreamRequest::SeekStream(
							voice.data.stream.streamContextIndex,
							stream.gen,
							voice.pendingSeekFrame
						));

						voice.data.stream.frontBufferIndex = 0;
						voice.hasPendingSeek = false;
						voice.pendingSeekFrame = 0;
					}
				}
			}

			// Check if we are waiting for and I/O operation
			if (voice.soundType == SoundType::Stream) {
				StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
				if (stream.state.load(std::memory_order_acquire) != StreamState::Streaming) return;
			}

			// Handle state change
			if (voice.currentState != voice.targetState) voice.currentState = voice.targetState;
		}

		if (!requiresSilence && voice.currentState == VoiceState::Playing) voice.targetFadeGain = 1.0f;
    }

    bool RtSystem::ProcessVoice(uint32_t voiceIndex, uint32_t frameCount) {
		Voice& voice = m_voicePool[voiceIndex];

        float* busBuffer = GetBusBuffer(m_busBufferPool.data(), voice.currentBusIndex, frameCount);
        uint32_t framesMixed = 0;

        while (framesMixed < frameCount) {
			const float* sourceData = nullptr;
        	uint32_t framesInSource = 0;
        	uint32_t sourceChannels = 0;
        	uint32_t cursorInt = static_cast<uint32_t>(voice.cursor);

			if (voice.soundType == SoundType::Resident) {
				sourceData = voice.data.resident.pcmData;
				framesInSource = voice.data.resident.frameCount;
				sourceChannels = voice.channels;
			}
			else if (voice.soundType == SoundType::Stream) {
				StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
				StreamState streamState = stream.state.load(std::memory_order_acquire);
				if (streamState != StreamState::Streaming) {
					// Stream could be preparing or seeking, check if it has failed
					if (streamState == StreamState::Error) {
						voice.currentState = VoiceState::Stopped;
						voice.exitCondition = PlaybackExitCondition::Error;
						return false;
					}

					// It's preparing or seeking
					return true;
				}

				if (!stream.bufferReady[voice.data.stream.frontBufferIndex].load(std::memory_order_acquire)) {
					// TODO: Send this as an event to the API thread for logging there
					DALIA_LOG_ERR(LOG_CTX_MIXER, "Stream buffer underrun.");
					return true;
				}

				sourceData = stream.buffers[voice.data.stream.frontBufferIndex];
				sourceChannels = stream.channels;

				// Determine the number of valid frames in the buffer
				const uint32_t eofIndex = stream.eofIndex[voice.data.stream.frontBufferIndex];
				if (!voice.isLooping && eofIndex != NO_EOF) {
					uint32_t samplesInSource = eofIndex;
					framesInSource = samplesInSource / stream.channels;
				}
				else {
					framesInSource = DOUBLE_BUFFER_FRAMES;
				}
			}
			else return false;

			uint32_t remainingFramesInSource = framesInSource - cursorInt;
        	if (cursorInt >= framesInSource) remainingFramesInSource = 0; // Safety clamp
			uint32_t framesToMixNow = std::min(frameCount - framesMixed, remainingFramesInSource);

        	if (framesToMixNow > 0) {
        		const float* DALIA_RESTRICT inPtr = &sourceData[cursorInt * sourceChannels];
        		float* DALIA_RESTRICT outPtr = &busBuffer[framesMixed * m_outChannels];

        		float localFadeGain = voice.currentFadeGain;
        		float localFadeTarget = voice.targetFadeGain;

        		for (uint32_t i = 0; i < framesToMixNow; i++) {
        			StepFadeGain(localFadeGain, localFadeTarget, m_fadeStep);

        			StepMatrixGains(voice.currentGainMatrix, voice.targetGainMatrix, sourceChannels,
        				m_outChannels, m_smoothingCoefficient);

        			for (uint32_t inC = 0; inC < sourceChannels; inC++) {
        				float sample = inPtr[inC] * localFadeGain; // Applying fade gain first

        				for (uint32_t outC = 0; outC < m_outChannels; outC++) {
        					outPtr[outC] += sample * voice.currentGainMatrix[inC][outC];
        				}
        			}

        			inPtr += sourceChannels;
        			outPtr += m_outChannels;
        		}

        		voice.currentFadeGain = localFadeGain;
        	}

        	voice.cursor += static_cast<float>(framesToMixNow);
			framesMixed += framesToMixNow;

			// Handle end of buffer
			if (static_cast<uint32_t>(voice.cursor) >= framesInSource) {
				if (voice.soundType == SoundType::Resident) {
					if (voice.isLooping) {
						voice.cursor = 0.0f; // Loop back to start
					}
					else {
						voice.currentState = VoiceState::Stopped;
						voice.exitCondition = PlaybackExitCondition::NaturalEnd;
						return false;
					}
				}
				else if (voice.soundType == SoundType::Stream) {
					StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];

					// Did we hit EOF?
					if (stream.eofIndex[voice.data.stream.frontBufferIndex] != NO_EOF && !voice.isLooping) {
						voice.currentState = VoiceState::Stopped;
						voice.exitCondition = PlaybackExitCondition::NaturalEnd;
						return false;
					}

					stream.bufferReady[voice.data.stream.frontBufferIndex].store(false, std::memory_order_release);
					const uint32_t gen = stream.gen.load(std::memory_order_relaxed);
					IoStreamRequest req = IoStreamRequest::RefillStreamBuffer(
						voice.data.stream.streamContextIndex,
						gen,
						voice.data.stream.frontBufferIndex
					);
					m_ioStreamRequests->Push(req);

					// Swap buffers
					voice.data.stream.frontBufferIndex = 1 - voice.data.stream.frontBufferIndex;
					voice.cursor = 0.0f;
				}
				else return false;
			}
		}

		return true;
    }

    void RtSystem::FreeVoice(uint32_t vIndex) {
    	Voice& voice = m_voicePool[vIndex];


    	if (voice.soundType == SoundType::Stream) {
    		m_ioStreamRequests->Push(IoStreamRequest::ReleaseStream(
    			voice.data.stream.streamContextIndex,
    			m_streamPool[voice.data.stream.streamContextIndex].gen
    		));
    	}

		// TODO: Remove these logs some time
    	if (voice.exitCondition == PlaybackExitCondition::NaturalEnd) {
    		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d finished naturally.", vIndex);
    	}
    	else if (voice.exitCondition == PlaybackExitCondition::ExplicitStop) {
    		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d stopped explicitly.", vIndex);
    	}
    	else if (voice.exitCondition == PlaybackExitCondition::Evicted) {
    		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d evicted.", vIndex);
    	}
    	else if (voice.exitCondition == PlaybackExitCondition::Error) {
    		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d stopped by error.", vIndex);
    	}

    	m_rtEvents->Push(RtEvent::VoiceStopped(vIndex, voice.gen, voice.exitCondition));
    	voice.Reset();
    }

    bool RtSystem::ResolveBus(Bus& bus) {
		bool requiresSilence = false;
		bool topologyChanged = false;

		// Do we need to duck?
		if (bus.currentParentIndex != bus.targetParentIndex) requiresSilence = true;

		if (requiresSilence) bus.targetFadeGain = GAIN_SILENCE;

		if (math::NearlyEqual(bus.currentFadeGain, 0.0f, EPSILON_GAIN)) {
			// Bus is silent

			if (bus.currentParentIndex != bus.targetParentIndex) {
				bus.currentParentIndex = bus.targetParentIndex;
				DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Re-routed bus to target");
				topologyChanged = true;
			}
		}

		if (!requiresSilence) bus.targetFadeGain = GAIN_DEFAULT;

		return topologyChanged;
    }

    void RtSystem::ProcessBus(uint32_t busIndex, uint32_t frameCount) {
    	Bus& bus = m_busPool[busIndex];
		float* buffer = GetBusBuffer(m_busBufferPool.data(), busIndex, frameCount);

		for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
			EffectSlot& slot = bus.effectSlots[i];
			if (slot.state == EffectState::None) continue;
			ApplyBusEffect(buffer, slot, frameCount);
		}

		ApplyGainAndFade(
			buffer, frameCount, m_outChannels,
			bus.currentGain, bus.targetGain, m_smoothingCoefficient,
			bus.currentFadeGain, bus.targetFadeGain, m_fadeStep
		);

    	if (bus.currentParentIndex != NO_PARENT) {
    		float* parentBuffer = m_busBufferPool.data() + (bus.currentParentIndex * frameCount * CHANNELS_MAX);
    		MixToBuffer(parentBuffer, buffer, frameCount * m_outChannels);
    	}
    }

    void RtSystem::ApplyBusEffect(float* busBuffer, EffectSlot& slot, uint32_t frameCount) {
		float fadeDurationInSeconds = 0.01f;
		float fadeDelta = 1.0f / (m_outSampleRate * fadeDurationInSeconds);
		uint32_t sampleCount = frameCount * m_outChannels;

		std::memcpy(m_dspScratchBuffer.data(), busBuffer, sampleCount * sizeof(float));
		switch (slot.effect.GetType()) {
			case EffectType::None: break;
			case EffectType::Biquad:
				BiquadFilter& biquad = m_biquadFilterPool[slot.effect.GetIndex()];
				ProcessBiquad(m_dspScratchBuffer.data(), frameCount, m_outChannels, biquad, m_outSampleRate);
				break;
		}

		if (slot.state == EffectState::Active) {
			std::memcpy(busBuffer, m_dspScratchBuffer.data(), sampleCount * sizeof(float));
		}
		else {
			float targetMix = 0.0f;
			if (slot.state == EffectState::FadingIn) targetMix = 1.0f;
			else if (slot.state == EffectState::FadingOut) targetMix = 0.0f;

			ApplyBlockCrossFade(
				busBuffer,
				m_dspScratchBuffer.data(),
				frameCount,
				m_outChannels,
				slot.currentMix,
				targetMix,
				fadeDelta
			);

			if (slot.state == EffectState::FadingIn && slot.currentMix >= 1.0f) {
				slot.state = EffectState::Active;
				m_rtEvents->Push(RtEvent::EffectActive(slot.effect.GetUUID()));
			}
			else if (slot.state == EffectState::FadingOut && slot.currentMix <= 0.0f) {
				slot.Reset(); // Detach effect
				m_rtEvents->Push(RtEvent::EffectDetached(slot.effect.GetUUID()));
			}
		}
    }

    void RtSystem::AttachEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot) {
		FlushEffect(effect.GetType(), effect.GetIndex(), effect.GetGeneration()); // Remove history

		EffectSlot& slot = m_busPool[busIndex].effectSlots[effectSlot];
		slot.effect = effect;
		slot.currentMix = 0.0f;
		slot.state = EffectState::FadingIn;
    }

    void RtSystem::DetachEffect(EffectHandle effect, uint32_t busIndex,
    	uint32_t effectSlot) {
		EffectSlot& slot = m_busPool[busIndex].effectSlots[effectSlot];
		if (slot.effect == effect) slot.Reset();
    }

    void RtSystem::FadeOutEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot) {
		EffectSlot& slot = m_busPool[busIndex].effectSlots[effectSlot];
		if (slot.effect == effect && slot.state != EffectState::None) slot.state = EffectState::FadingOut;
    }

    void RtSystem::FlushEffect(EffectType type, uint32_t index, uint32_t gen) {
		switch (type) {
		case EffectType::None: break;
		case EffectType::Biquad:
			if (m_biquadFilterPool[index].generation == gen) m_biquadFilterPool[index].Flush();
			break;
		default: break;
		}
    }
}
