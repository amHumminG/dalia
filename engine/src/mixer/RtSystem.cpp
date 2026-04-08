#include "RtSystem.h"

#include "core/Logger.h"
#include "core/Constants.h"
#include "core/Math.h"

#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"

#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoStreamRequestQueue.h"

#include "dalia/audio/SoundControl.h"

#include <cmath>

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

				if (std::abs(diff) < GAIN_EPSILON) current[inC][outC] = target[inC][outC];
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

	static inline void ApplyVolume(float* DALIA_RESTRICT buffer, uint32_t frameCount, uint32_t channels,
		float& currentVolume, float targetVolume, float smoothingCoefficient) {

		// --- No volume movement ---
		if (NearlyEqual(currentVolume, targetVolume, VOLUME_EPSILON)) {
			currentVolume = targetVolume;

			if (NearlyEqual(currentVolume, 1.0f, VOLUME_EPSILON)) return;

			uint32_t sampleCount = frameCount * channels;
			if (NearlyEqual(currentVolume, 0.0f, VOLUME_EPSILON)) {
				std::memset(buffer, 0, sampleCount * sizeof(float));
				return;
			}

			for (uint32_t i = 0; i < sampleCount; i++) buffer[i] *= currentVolume;
			return;
		}

		// --- Smoothing current to target ---
		for (uint32_t i = 0; i < frameCount; i++) {
			currentVolume += (targetVolume - currentVolume) * smoothingCoefficient;

			for (uint32_t c = 0; c < channels; c++) {
				buffer[(i * channels) + c] *= currentVolume;
			}
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

	// ------------

    RtSystem::RtSystem(const RtSystemConfig& config)
        : m_outputChannels(config.outputChannels),
		m_outputSampleRate(config.outputSampleRate),
		m_voicePool(config.voicePool),
        m_streamPool(config.streamPool),
        m_busPool(config.busPool),
		m_busBufferPool(config.busBufferPool),
        m_masterBus(config.masterBus),
        m_rtCommands(config.rtCommands),
        m_rtEvents(config.rtEvents),
        m_ioStreamRequests(config.ioStreamRequests),
		m_dspScratchBuffer(config.dspScratchBuffer),
		m_biquadFilterPool(config.biquadFilterPool) {
        // Empty bus graph
        m_activeMixOrder = std::span<const uint32_t>();
		m_smoothingCoefficient = 1.0f - std::exp(-2.0f * PI * SMOOTHING_CUTOFF_HZ / config.outputSampleRate);
		m_fadeStep = CalculateLinearFadeStep(GAIN_FADE_TIME, m_outputSampleRate);
    }

    void RtSystem::OnAudioCallback(float* output, uint32_t frameCount) {
        // Process incoming commands from the API thread
        ProcessCommands();

        // Render the audio frame
        Render(output, frameCount);
    }

    void RtSystem::ProcessCommands() {
        RtCommand cmd;
        while (m_rtCommands->Pop(cmd)) {
            switch (cmd.type) {
	            case RtCommand::Type::SwapMixOrder: {
            		m_activeMixOrder = std::span<const uint32_t>(
						cmd.data.mixOrder.ptr,
						cmd.data.mixOrder.nodeCount
					);

            		// Send event to acknowledge the swap
            		m_rtEvents->Push(RtEvent::MixOrderSwapped());
	            	break;
	            }
				case RtCommand::Type::AllocateVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];

	            	voice.gen = cmd.targetGen;
	            	voice.currentState = VoiceState::Inactive;
	            	voice.targetState = VoiceState::Inactive;
		            break;
	            }
				case RtCommand::Type::DeallocateVoice: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen) break; // TODO: Should we really check generation here?

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
				case RtCommand::Type::SetVoiceGainMatrix: {
	            	Voice& voice = m_voicePool[cmd.targetIndex];
	            	if (voice.gen != cmd.targetGen || voice.isExiting) break;

					std::memcpy(voice.targetGainMatrix, cmd.data.gain.gainMatrix,
						sizeof(voice.targetGainMatrix));
					break;
	            }
				case RtCommand::Type::AllocateBus: {
		            uint32_t bIndex = cmd.targetIndex;
	            	uint32_t bIndexParent = cmd.data.setParent.parentIndex;

	            	m_busPool[bIndex].parentBusIndex = bIndexParent;
	            	break;
	            }
				case RtCommand::Type::DeallocateBus: {
	            	uint32_t bIndex = cmd.targetIndex;

	            	m_busPool[bIndex].Reset();
	            	break;
				}
				case RtCommand::Type::SetBusParent: {
	            	uint32_t bIndex = cmd.targetIndex;
	            	uint32_t bIndexParent = cmd.data.setParent.parentIndex;

	            	m_busPool[bIndex].parentBusIndex = bIndexParent;
		            break;
	            }
				case RtCommand::Type::SetBusVolume: {
	            	uint32_t bIndex = cmd.targetIndex;
	            	float newVolume = cmd.data.floatVal.value;

	            	m_busPool[bIndex].targetVolumeLinear = newVolume;
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
	            	CalculateBiquadCoefficients(biquad, static_cast<float>(m_outputSampleRate));
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

    void RtSystem::Render(float* output, uint32_t frameCount) {
        const uint32_t sampleCount = frameCount * m_outputChannels;

    	// Clear all bus buffers
        std::memset(m_busBufferPool.data(), 0, m_busBufferPool.size());

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

        	ReconcileVoice(voice);

        	if (voice.currentState == VoiceState::Playing) {
            	bool isStillPlaying = ProcessVoice(vIndex, frameCount);
            	if (!isStillPlaying) voice.currentState = VoiceState::Stopped;

            	voicesMixed++;
            }
        }

    	if (voicesMixed == 0) return; // Early return to save compute power

        // --- Bus Pass ---
        for (uint32_t bIndex : m_activeMixOrder) {
            Bus& bus = m_busPool[bIndex];

            // DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Mixing bus %d -> bus %d.", bIndex, bus.parentBusIndex);
            ProcessBus(bIndex, frameCount);
        }

        float* masterBuffer = m_busBufferPool.data();
		ApplySoftClipper(masterBuffer, sampleCount);
        std::copy_n(masterBuffer, sampleCount, output);
    }

    bool RtSystem::ProcessVoice(uint32_t voiceIndex, uint32_t frameCount) {
		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Processing voice %d.", voiceIndex);
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
        		float* DALIA_RESTRICT outPtr = &busBuffer[framesMixed * m_outputChannels];

        		float localFadeGain = voice.currentFadeGain;
        		float localFadeTarget = voice.targetFadeGain;

        		for (uint32_t i = 0; i < framesToMixNow; i++) {
        			StepFadeGain(localFadeGain, localFadeTarget, m_fadeStep);

        			StepMatrixGains(voice.currentGainMatrix, voice.targetGainMatrix, sourceChannels,
        				m_outputChannels, m_smoothingCoefficient);

        			for (uint32_t inC = 0; inC < sourceChannels; inC++) {
        				float sample = inPtr[inC] * localFadeGain; // Applying fade gain first

        				for (uint32_t outC = 0; outC < m_outputChannels; outC++) {
        					outPtr[outC] += sample * voice.currentGainMatrix[inC][outC];
        				}
        			}

        			inPtr += sourceChannels;
        			outPtr += m_outputChannels;
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

    void RtSystem::ReconcileVoice(Voice& voice) {
		bool requiresSilence = false;

		if (voice.currentState != voice.targetState && voice.targetState != VoiceState::Playing) requiresSilence = true;
		if (voice.currentBusIndex != voice.targetBusIndex) requiresSilence = true;
		if (voice.hasPendingSeek) requiresSilence = true;

		if (requiresSilence) voice.targetFadeGain = 0.0f;

		if (NearlyEqual(voice.currentFadeGain, 0.0f, GAIN_EPSILON) || voice.currentState != VoiceState::Playing) {
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

    void RtSystem::FreeVoice(uint32_t vIndex) {
    	Voice& voice = m_voicePool[vIndex];


    	if (voice.soundType == SoundType::Stream) {
    		m_ioStreamRequests->Push(IoStreamRequest::ReleaseStream(voice.data.stream.streamContextIndex));
    	}

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

    void RtSystem::ProcessBus(uint32_t busIndex, uint32_t frameCount) {
    	Bus& bus = m_busPool[busIndex];
		float* buffer = GetBusBuffer(m_busBufferPool.data(), busIndex, frameCount);

		for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
			EffectSlot& slot = bus.effectSlots[i];
			if (slot.state == EffectState::None) continue;
			ApplyBusEffect(buffer, slot, frameCount);
		}

    	// Apply volume
		ApplyVolume(buffer, frameCount, m_outputChannels, bus.currentVolumeLinear, bus.targetVolumeLinear,
			m_smoothingCoefficient);

    	if (bus.parentBusIndex != NO_PARENT) {
    		float* parentBuffer = m_busBufferPool.data() + (bus.parentBusIndex * frameCount * CHANNELS_MAX);
    		MixToBuffer(parentBuffer, buffer, frameCount * m_outputChannels);
    	}
    }

    void RtSystem::ApplyBusEffect(float* busBuffer, EffectSlot& slot, uint32_t frameCount) {
		float fadeDurationInSeconds = 0.01f;
		float fadeDelta = 1.0f / (m_outputSampleRate * fadeDurationInSeconds);
		uint32_t sampleCount = frameCount * m_outputChannels;

		std::memcpy(m_dspScratchBuffer.data(), busBuffer, sampleCount * sizeof(float));
		switch (slot.effect.GetType()) {
			case EffectType::None: break;
			case EffectType::Biquad:
				BiquadFilter& biquad = m_biquadFilterPool[slot.effect.GetIndex()];
				ProcessBiquad(m_dspScratchBuffer.data(), frameCount, m_outputChannels, biquad, m_outputSampleRate);
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
				m_outputChannels,
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
