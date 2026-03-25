#include "RtSystem.h"

#include "core/Logger.h"

#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"

#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoStreamRequestQueue.h"

#include "dalia/audio/SoundControl.h"

#include <cmath>

#include "effects/Biquad.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

namespace dalia {

    RtSystem::RtSystem(const RtSystemConfig& config)
        : m_voicePool(config.voicePool),
        m_streamPool(config.streamPool),
        m_busPool(config.busPool),
        m_masterBus(config.masterBus),
        m_rtCommands(config.rtCommands),
        m_rtEvents(config.rtEvents),
        m_ioStreamRequests(config.ioStreamRequests),
		m_biquadPool(config.biquadPool) {
        // Empty bus graph
        m_activeMixOrder = std::span<const uint32_t>();
    }

    void RtSystem::OnAudioCallback(float* output, uint32_t frameCount, uint32_t channels) {
        // Process incoming commands from the API thread
        ProcessCommands();

        // Render the audio frame
        Render(output, frameCount, channels);
    }

    void RtSystem::ProcessCommands() {
        RtCommand cmd;
        // TODO: We should probably set a limit on the amount of commands we process in one audio frame
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
	            case RtCommand::Type::AllocateVoiceResident: {
	            	uint32_t vIndex = cmd.data.prepResident.voiceIndex;
	            	Voice& voice = m_voicePool[vIndex];

	            	// Voice setup
	            	voice.soundType = SoundType::Resident;
	            	voice.state = VoiceState::Inactive;
	            	voice.generation = cmd.data.prepResident.voiceGeneration;

	            	voice.data.resident.pcmData = cmd.data.prepResident.pcmData;
	            	voice.data.resident.frameCount = cmd.data.prepResident.frameCount;

	            	voice.channels = cmd.data.prepResident.channels;
	            	voice.sampleRate = cmd.data.prepResident.sampleRate;

	            	DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Prepared resident voice %d with %d frames",
	            		vIndex, cmd.data.prepResident.frameCount);

	            	break;
	            }
				case RtCommand::Type::AllocateVoiceStreaming: {
	            	uint32_t vIndex = cmd.data.prepStreaming.voiceIndex;
	            	Voice& voice = m_voicePool[vIndex];

	            	// Voice setup
	            	voice.soundType = SoundType::Stream;
	            	voice.state = VoiceState::Inactive;
	            	voice.generation = cmd.data.prepStreaming.voiceGeneration;

	            	voice.data.stream.streamContextIndex = cmd.data.prepStreaming.streamIndex;
	            	voice.data.stream.frontBufferIndex = 0;

	            	break;
	            }
				case RtCommand::Type::SetVoiceParent: {
	            	uint32_t vIndex = cmd.data.voiceParent.voiceIndex;
	            	uint32_t vGen = cmd.data.voiceParent.voiceGeneration;
	            	uint32_t bIndexParent = cmd.data.voiceParent.parentBusIndex;
	            	Voice& voice = m_voicePool[vIndex];

	            	// Check for outdated command
	            	if (voice.generation != vGen) break;

	            	voice.parentBusIndex = bIndexParent;
	            	break;
				}
				case RtCommand::Type::PlayVoice: {
	            	uint32_t vIndex = cmd.data.voice.voiceIndex;
	            	uint32_t vGen = cmd.data.voice.voiceGeneration;
	            	Voice& voice = m_voicePool[vIndex];

	            	// Check for outdated command
	            	if (voice.generation != vGen) break;

	            	if (voice.state == VoiceState::Inactive || voice.state == VoiceState::Paused) {
	            		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d set to playing", vIndex);
	            		voice.state = VoiceState::Playing;
	            	}

	            	break;
				}
                case RtCommand::Type::PauseVoice: {
	            	uint32_t vIndex = cmd.data.voice.voiceIndex;
	            	uint32_t vGen = cmd.data.voice.voiceGeneration;
	            	Voice& voice = m_voicePool[vIndex];

	            	// Check for outdated command
	            	if (voice.generation != vGen) break;

	            	if (voice.state == VoiceState::Playing) {
	            		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d set to paused", vIndex);
	            		voice.state = VoiceState::Paused;
	            	}

	            	break;
                }
                case RtCommand::Type::StopVoice: {
	            	uint32_t vIndex = cmd.data.voice.voiceIndex;
	            	uint32_t vGen = cmd.data.voice.voiceGeneration;
	            	Voice& voice = m_voicePool[vIndex];

	            	// Check for outdated command
	            	if (voice.generation != vGen) break;

	            	voice.state = VoiceState::Stopped;
	            	voice.exitCondition = PlaybackExitCondition::ExplicitStop;
	            	break;
                }
				case RtCommand::Type::AllocateBus: {
		            uint32_t bIndex = cmd.data.bus.busIndex;
	            	uint32_t bIndexParent = cmd.data.bus.parentBusIndex;

	            	m_busPool[bIndex].m_parentBusIndex = bIndexParent;
	            	break;
	            }
				case RtCommand::Type::DeallocateBus: {
	            	uint32_t bIndex = cmd.data.bus.busIndex;

	            	m_busPool[bIndex].Reset();
	            	break;
				}
				case RtCommand::Type::SetBusParent: {
	            	uint32_t bIndex = cmd.data.bus.busIndex;
	            	uint32_t bIndexParent = cmd.data.bus.parentBusIndex;

	            	m_busPool[bIndex].m_parentBusIndex = bIndexParent;
		            break;
	            }
				case RtCommand::Type::SetBusVolume: {
	            	uint32_t bIndex = cmd.data.busFloat.busIndex;
	            	float newVolume = cmd.data.busFloat.value;

	            	m_busPool[bIndex].m_volumeLinear = newVolume;
	            	break;
				}
				case RtCommand::Type::AllocateBiquad: {
	            	Biquad& biquad = m_biquadPool[cmd.data.biquad.index];
	            	biquad.generation = cmd.data.biquad.gen;
	            	biquad.type = cmd.data.biquad.type;

					biquad.targetFrequency = cmd.data.biquad.config.frequency;
					biquad.currentFrequency = cmd.data.biquad.config.frequency;
	            	biquad.targetQ = cmd.data.biquad.config.q;
	            	biquad.currentQ = cmd.data.biquad.config.q;

	            	CalculateBiquadCoefficients(biquad, m_outputSampleRate);

	            	break;
				}
				case RtCommand::Type::SetBiquadParams: {
	            	Biquad& biquad = m_biquadPool[cmd.data.biquad.index];

	            	if (biquad.generation != cmd.data.biquad.gen) break;

	            	biquad.targetFrequency = cmd.data.biquad.config.frequency;
	            	biquad.targetQ = cmd.data.biquad.config.q;

	            	break;
				}
                default: break;
            }
        }
    }

    void RtSystem::Render(float* output, uint32_t frameCount, uint32_t channels) {
        const uint32_t sampleCount = frameCount * channels;

    	// Clear bus buffers
        for (uint32_t bIndex : m_activeMixOrder) m_busPool[bIndex].Clear();

        // --- Voice Pass ---
    	uint32_t voicesMixed = 0;
        for (uint32_t i = 0; i < m_voicePool.size(); i++) {
            Voice& voice = m_voicePool[i];
        	if (voice.state == VoiceState::Stopped)	FreeVoice(i);
        	else if (voice.parentBusIndex == NO_PARENT) { // Maybe this check should happen on the API thread instead?
        		voice.exitCondition = PlaybackExitCondition::Evicted;
        		FreeVoice(i);
        	}
            else if (voice.state == VoiceState::Playing) {
            	DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Mixing voice %d -> bus %d.", i, voice.parentBusIndex);
            	bool isStillPlaying = MixVoiceToBus(voice, voice.parentBusIndex, frameCount);
            	if (!isStillPlaying) voice.state = VoiceState::Stopped;

            	voicesMixed++;
            }
        }

    	if (voicesMixed == 0) return; // Early return to save compute power

        // --- Bus Pass ---
        for (uint32_t bIndex : m_activeMixOrder) {
            Bus& bus = m_busPool[bIndex];
            bus.ApplyDSP(sampleCount);

            uint32_t bIndexParent = bus.m_parentBusIndex;
            if (bIndexParent != NO_PARENT) {
            	DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Mixing bus %d -> bus %d.", bIndex, bIndexParent);
                Bus& parentBus = m_busPool[bIndexParent];
                parentBus.MixInBuffer(bus.GetBuffer(), sampleCount);
            }
        }

        // Final Output (clamped between -1.0f and 1.0f) We probably want a soft limiter for this in the future
        auto masterBuffer = m_masterBus->GetBuffer();
        for (uint32_t i = 0; i < sampleCount; i++) {
            masterBuffer[i] = std::clamp(masterBuffer[i], -1.0f, 1.0f);
        }
        std::copy_n(masterBuffer.data(), sampleCount, output);
    }

    bool RtSystem::MixVoiceToBus(Voice& voice, uint32_t bIndex, uint32_t frameCount) {
        std::span<float> busBuffer = m_busPool[bIndex].GetBuffer();
        uint32_t framesMixed = 0;

    	// --- Panning & DSP ---
    	const float panNormalized = (voice.pan + 1.0f) * 0.5f;
    	const float angle = panNormalized * static_cast<float>(M_PI_2); // Angle between 0 and PI/2 radians
    	const float gainL = std::cos(angle) * voice.volumeLinear;
    	const float gainR = std::sin(angle) * voice.volumeLinear;

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
					// Stream could be preparing still, check if it has failed
					if (streamState == StreamState::Error) {
						voice.state = VoiceState::Stopped;
						voice.exitCondition = PlaybackExitCondition::Error;
						return false;
					}

					// It's still preparing
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
        		float* outPtr = &busBuffer[framesMixed * 2]; // 2 for stereo bus
        		uint32_t sourceStride = sourceChannels; // TODO: This is where we have to resample in the future
        		if (sourceChannels == 1) {
        			for (uint32_t i = 0; i < framesToMixNow; i++) {
        				float sample = sourceData[(cursorInt + i) * sourceStride];

        				// Stereo mix
        				outPtr[0] += sample * gainL;
        				outPtr[1] += sample * gainR;

        				outPtr += 2; // 2 for stereo bus
        			}
        		}
        		else if (sourceChannels == 2) {
        			for (uint32_t i = 0; i < framesToMixNow; i++) {
        				float sampleL = sourceData[(cursorInt + i) * 2 + 0];
        				float sampleR = sourceData[(cursorInt + i) * 2 + 1];

        				// Stereo mix
        				outPtr[0] += sampleL * gainL;
        				outPtr[1] += sampleR * gainR;

        				outPtr += 2; // 2 for stereo bus
        			}
        		}
        		else {
        			DALIA_LOG_WARN(LOG_CTX_MIXER, "Failed to mix voice with invalid channel count (%d).",
        				sourceChannels);
        		}
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
						voice.state = VoiceState::Stopped;
						voice.exitCondition = PlaybackExitCondition::NaturalEnd;
						return false;
					}
				}
				else if (voice.soundType == SoundType::Stream) {
					StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];

					// Did we hit EOF?
					if (stream.eofIndex[voice.data.stream.frontBufferIndex] != NO_EOF && !voice.isLooping) {
						voice.state = VoiceState::Stopped;
						voice.exitCondition = PlaybackExitCondition::NaturalEnd;
						return false;
					}

					stream.bufferReady[voice.data.stream.frontBufferIndex].store(false, std::memory_order_release);
					const uint32_t gen = stream.generation.load(std::memory_order_relaxed);
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

    	m_rtEvents->Push(RtEvent::VoiceStopped(vIndex, voice.generation, voice.exitCondition));
    	voice.Reset();
    }
}
