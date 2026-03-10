#include "systems/RealTimeSystem.h"

#include "core/Logger.h"

#include "mixer/Voice.h"
#include "mixer/StreamingContext.h"
#include "mixer/Bus.h"

#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoRequestQueue.h"

#include <cmath>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

namespace dalia {

    RealTimeSystem::RealTimeSystem(const RealTimeSystemConfig& config)
        : m_voicePool(config.voicePool),
        m_streamPool(config.streamPool),
        m_busPool(config.busPool),
        m_masterBus(config.masterBus),
        m_rtCommandQueue(config.rtCommandQueue),
        m_rtEventQueue(config.rtEventQueue),
        m_ioRequestQueue(config.ioRequestQueue) {
        // Empty bus graph
        m_activeMixOrder = std::span<const uint32_t>();
    }

    void RealTimeSystem::OnAudioCallback(float* output, uint32_t frameCount, uint32_t channels) {
        // Process incoming commands from the game thread
        ProcessCommands();

        // Render the audio frame
        Render(output, frameCount, channels);
    }

    void RealTimeSystem::ProcessCommands() {
        RtCommand cmd;
        // TODO: We should probably set a limit on the amount of commands we process in one audio frame
        while (m_rtCommandQueue->Pop(cmd)) {
            switch (cmd.type) {
	            case RtCommand::Type::SwapMixOrder: {
            		m_activeMixOrder = std::span<const uint32_t>(
						cmd.data.mixOrder.ptr,
						cmd.data.mixOrder.nodeCount
					);

            		// Send event to acknowledge the swap
            		m_rtEventQueue->Push(RtEvent::MixOrderSwapped());
	            }
                case RtCommand::Type::PlayVoice: {
                    // TODO: Implement
                }
                case RtCommand::Type::PauseVoice: {
                    // TODO: Implement
                }
                case RtCommand::Type::StopVoice: {
                    // TODO: Implement
                }
                default: break;
            }
        }
    }

    void RealTimeSystem::Render(float* output, uint32_t frameCount, uint32_t channels) {
        const uint32_t sampleCount = frameCount * channels;

        for (uint32_t busIndex : m_activeMixOrder) {
            m_busPool[busIndex].Clear();
        }

        // --- Voice Pass --- (Parallel ready when the time comes)
        for (uint32_t i = 0; i < m_voicePool.size(); i++) {
            Voice& voice = m_voicePool[i];
            if (voice.state != VoiceState::Playing) continue;

            bool isStillPlaying = MixVoiceToBus(voice, voice.parentBusIndex, frameCount);
            if (!isStillPlaying) {
                // TODO: We need to determine why the voice finished here or inside MixVoiceToBus and send the approperiate event!
                m_rtEventQueue->Push(RtEvent::VoiceFinished(i, voice.generation));
                voice.Reset();
            }
        }

        // --- Bus Pass --- (Not yet parallel ready)
        for (uint32_t busIndex : m_activeMixOrder) {
            Bus& bus = m_busPool[busIndex];
            bus.ApplyDSP(sampleCount);

            // Maybe we just replace this with a bus->MixToParent(uint32_t sampleCount)?
            uint32_t parentIndex = bus.parentIndex;
            if (parentIndex != NO_PARENT) {
                Bus& parentBus = m_busPool[parentIndex];
                parentBus.MixInBuffer(bus.GetBuffer(), sampleCount);
            }
        }

        // Final Output (clamped between -1.0f and 1.0f
        auto masterBuffer = m_masterBus->GetBuffer();
        for (uint32_t i = 0; i < sampleCount; i++) {
            masterBuffer[i] = std::clamp(masterBuffer[i], -1.0f, 1.0f);
        }
        std::copy_n(masterBuffer.data(), sampleCount, output);
    }

    bool RealTimeSystem::MixVoiceToBus(Voice& voice, uint32_t busIndex, uint32_t frameCount) {
        std::span<float> busBuffer = m_busPool[busIndex].GetBuffer();
        uint32_t framesMixed = 0;

    	// --- Panning & DSP ---
    	const float panNormalized = (voice.pan + 1.0f) * 0.5f;
    	const float angle = panNormalized * static_cast<float>(M_PI_2); // Angle between 0 and PI/2 radians
    	const float gainL = std::cos(angle) * voice.volume;
    	const float gainR = std::sin(angle) * voice.volume;

        while (framesMixed < frameCount) {
			float* sourceData = nullptr;
        	uint32_t framesInSource = 0;
        	uint32_t cursorInt = static_cast<uint32_t>(voice.cursor);

			if (voice.sourceType == VoiceSourceType::Resident) {
				sourceData = voice.buffer.data();
				framesInSource = static_cast<uint32_t>(voice.buffer.size() / voice.channels);
			}
			else if (voice.sourceType == VoiceSourceType::Stream) {
				StreamingContext& stream = m_streamPool[voice.streamingContextIndex];
				if (stream.state.load(std::memory_order_acquire) != StreamState::Streaming) {
					// Voice is not ready to stream yet
					return true;
				}
				if (!stream.bufferReady[voice.frontBufferIndex].load(std::memory_order_acquire)) {
					// Buffer is not ready yet (it should be)
					Logger::Log(LogLevel::Error, "RtSystem", "Stream buffer underrun");
					return true;
				}

				sourceData = stream.buffers[voice.frontBufferIndex];

				// Determine the number of valid frames in the buffer
				const uint32_t eofIndex = stream.eofIndex[voice.frontBufferIndex];
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
        		uint32_t sourceStride = voice.channels; // TODO: This is where we have to resample in the future
        		if (voice.channels == 1) {
        			for (uint32_t i = 0; i < framesToMixNow; i++) {
        				float sample = sourceData[(cursorInt + i) * sourceStride];

        				// Stereo mix
        				outPtr[0] += sample * gainL;
        				outPtr[1] += sample * gainR;

        				outPtr += 2; // 2 for stereo bus
        			}
        		}
        		else if (voice.channels == 2) {
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
        			Logger::Log(LogLevel::Warning, "RtSystem",
						"Failed to mix voice with invalid channel count: %d", voice.channels);
        		}
        	}

        	voice.cursor += static_cast<float>(framesToMixNow);
			framesMixed += framesToMixNow;

			// Handle end of buffer
			if (static_cast<uint32_t>(voice.cursor) >= framesInSource) {
				if (voice.sourceType == VoiceSourceType::Resident) {
					if (voice.isLooping) {
						voice.cursor = 0.0f; // Loop back to start
					}
					else {
						voice.state = VoiceState::Stopping;
						return false;
					}
				}
				else if (voice.sourceType == VoiceSourceType::Stream) {
					StreamingContext& stream = m_streamPool[voice.streamingContextIndex];

					// Did we hit EOF?
					if (stream.eofIndex[voice.frontBufferIndex] != NO_EOF) {
						if (!voice.isLooping) {
							voice.state = VoiceState::Stopping;
							// Request stream release
							stream.generation.fetch_add(1, std::memory_order_relaxed);
							m_ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));
							return false;
						}
					}

					stream.bufferReady[voice.frontBufferIndex].store(false, std::memory_order_release);
					const uint32_t gen = stream.generation.load(std::memory_order_relaxed);
					m_ioRequestQueue->Push(IoRequest::RefillStreamBuffer(voice.streamingContextIndex, gen, voice.frontBufferIndex));

					// Swap buffers
					voice.frontBufferIndex = 1 - voice.frontBufferIndex;
					voice.cursor = 0.0f;
				}
				else return false;
			}
		}

		return true;
    }
}
