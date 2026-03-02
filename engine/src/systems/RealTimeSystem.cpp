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
        m_activeBusGraph = std::span<const uint32_t>();
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
                case RtCommand::Type::Play: {
                    // TODO: Implement
                }
                case RtCommand::Type::Pause: {
                    // TODO: Implement
                }
                case RtCommand::Type::Stop: {
                    // TODO: Implement
                }
                case RtCommand::Type::SwapGraph: {
                    m_activeBusGraph = std::span<const uint32_t>(
                        cmd.data.graph.ptr, // FIXME: This command should supply a pointer to the graph
                        cmd.data.graph.nodeCount
                    );

                    // Send event to acknowledge the swap
                    m_rtEventQueue->Push(RtEvent::GraphSwapped());
                }
                default: break;
            }
        }
    }

    void RealTimeSystem::Render(float* output, uint32_t frameCount, uint32_t channels) {
        const uint32_t sampleCount = frameCount * channels;

        for (uint32_t busIndex : m_activeBusGraph) {
            m_busPool[busIndex].Clear();
        }

        // --- Voice Pass --- (Parallel ready when the time comes)
        for (uint32_t i = 0; i < m_voicePool.size(); i++) {
            Voice& voice = m_voicePool[i];
            if (!voice.isPlaying) continue;

            bool isStillPlaying = MixVoiceToBus(voice, voice.parentBusIndex, frameCount);
            if (!isStillPlaying) {
                // Voice finished
                m_rtEventQueue->Push(RtEvent::VoiceFinished(i));
                voice.Reset();
            }
        }

        // --- Bus Pass --- (Not yet parallel ready)
        for (uint32_t busIndex : m_activeBusGraph) {
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

        while (framesMixed < frameCount) {
			uint32_t framesInSource = 0;
			float* sourceData = nullptr;

			if (voice.sourceType == VoiceSourceType::Resident) {
				sourceData = voice.buffer.data();
				framesInSource = static_cast<uint32_t>(voice.buffer.size() / voice.channels);
			}
			else if (voice.sourceType == VoiceSourceType::Stream) {
				StreamingContext& stream = m_streamPool[voice.streamingContextIndex];
				sourceData = stream.buffers[stream.frontBufferIndex];

				// Check if we hit EOF this buffer and on what index
				const uint32_t eofIndex = stream.eofIndex[stream.frontBufferIndex];
				if (!voice.isLooping && eofIndex != NO_EOF) {
					framesInSource = eofIndex;
				}
				else {
					framesInSource = DOUBLE_BUFFER_CHUNK_SIZE;
				}
			}
			else return false;

			uint32_t remainingInSource = framesInSource - static_cast<uint32_t>(voice.cursor);
			uint32_t framesToProcessNow = std::min(frameCount - framesMixed, remainingInSource);

			// --- Panning & DSP ---
			const float panNormalized = (voice.pan + 1.0f) * 0.5f;
			const float angle = panNormalized * static_cast<float>(M_PI_2); // Angle between 0 and PI/2 radians
			const float gainL = std::cos(angle) * voice.volume;
			const float gainR = std::sin(angle) * voice.volume;

			for (uint32_t i = 0; i < framesToProcessNow; i++) {
				float sample = sourceData[static_cast<size_t>(voice.cursor) * voice.channels];
				uint32_t targetIndex = (framesMixed + i) * 2; // 2 Channels
				busBuffer[targetIndex + 0] += sample * gainL;
				busBuffer[targetIndex + 1] += sample * gainR;

				voice.cursor += 1.0f;
			}
			framesMixed += framesToProcessNow;

			// Handle end of buffer
			if (static_cast<uint32_t>(voice.cursor) >= framesInSource) {
				if (voice.sourceType == VoiceSourceType::Resident) {
					if (voice.isLooping) {
						voice.cursor = 0.0f;
					}
					else {
						// Voice is no longer used -> slot should be free now (return false?)
						voice.isPlaying = false;
						return false;
					}
				}
				else if (voice.sourceType == VoiceSourceType::Stream) {
					StreamingContext& stream = m_streamPool[voice.streamingContextIndex];

					if (framesInSource < DOUBLE_BUFFER_CHUNK_SIZE) {
						// Natural end of file -> Kill voice
						// TODO: The two rows below should probably be part of some kind of releaseVoice function later on
						stream.generation.fetch_add(1, std::memory_order_relaxed);
						m_ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));

						return false;
					}

					stream.bufferReady[stream.frontBufferIndex].store(false, std::memory_order_relaxed);
					const uint32_t gen = stream.generation.load(std::memory_order_relaxed);
					m_ioRequestQueue->Push(IoRequest::RefillStreamBuffer(voice.streamingContextIndex, gen, stream.frontBufferIndex));

					// Swap buffers
					stream.frontBufferIndex = 1 - stream.frontBufferIndex;
					voice.cursor = 0.0f;

					if (!stream.bufferReady[stream.frontBufferIndex].load(std::memory_order_acquire)) {
						Logger::Log(LogLevel::Critical, "Voice (Streaming) Mixer", "Buffer underflow. Killing Voice...");

						// TODO: The two rows below should probably be part of some kind of releaseVoice function later on
						stream.generation.fetch_add(1, std::memory_order_relaxed);
						m_ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));

						return false;
					}
				}
				else return false;
			}
		}

		return true;
    }
}
