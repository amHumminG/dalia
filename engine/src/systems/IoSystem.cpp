#include "systems/IoSystem.h"

#include "core/Logger.h"

#include "core/SPSCRingBuffer.h"
#include "messaging/IoRequestQueue.h"
#include "mixer/StreamingContext.h"

#include "stb_vorbis.c"

#include <chrono>

namespace dalia {

    IoSystem::IoSystem(const IoSystemConfig& config)
        : m_ioRequestQueue(config.ioRequestQueue),
        m_streamPool(config.streamPool),
        m_freeStreams(config.freeStreams) {
    }

    IoSystem::~IoSystem() {
        Stop();
    }

    void IoSystem::Start() {
        if (m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(true, std::memory_order_release);
        m_ioThread = std::thread(&IoSystem::IoThreadMain, this);
    }

    void IoSystem::Stop() {
        if (!m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(false, std::memory_order_release);
        if (m_ioThread.joinable()) m_ioThread.join();
    }

    void IoSystem::IoThreadMain() {
        while (m_isRunning.load(std::memory_order_relaxed)) {
            bool didWork = false;
            IoRequest req;

            while (m_ioRequestQueue->Pop(req)) {
                didWork = true;

                // Handle requests
                switch (req.type) {
                    case IoRequest::Type::PrepareStream: {
                        StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];
                        if (stream.state.load(std::memory_order_acquire) != StreamState::Preparing) break;

                        if (stream.decoder) {
                            stb_vorbis_close(stream.decoder);
                            stream.decoder = nullptr;
                        }

                        int error;
                        stream.decoder = stb_vorbis_open_filename(req.data.stream.path, &error, nullptr);

                        if (stream.decoder) {
                            stb_vorbis_info info = stb_vorbis_get_info(stream.decoder);

                            // --- Check for unsupported formats ---
                            bool isSupported = true;

                            // Channels
                            if (info.channels == 0 || info.channels > 2) {
                                Logger::Log(LogLevel::Error, "IoSystem",
                                    "Failed to load file: %s. Unsupported channel count (%d) in file.",
                                    req.data.stream.path, info.channels);
                                isSupported = false;
                            }

                            // Sample rate
                            static constexpr uint32_t ENGINE_SAMPLE_RATE = 48000;
                            if (info.sample_rate != ENGINE_SAMPLE_RATE) {
                                Logger::Log(LogLevel::Error, "IoSystem",
                                    "Failed to load file: %s. Unsupported sample rate (%d).",
                                    req.data.stream.path, info.sample_rate);
                                isSupported = false;
                            }

                            if (!isSupported) {
                                stb_vorbis_close(stream.decoder);
                                stream.decoder = nullptr;
                                stream.state.store(StreamState::Error, std::memory_order_release);
                                break;
                            }

                            stream.channels = info.channels;
                            stream.sampleRate = info.sample_rate;
                            FillBuffer(stream, 0);
                            FillBuffer(stream, 1);
                            stream.state.store(StreamState::Streaming);

                            Logger::Log(LogLevel::Debug, "IoSystem", "Finished preparing stream %d for file: %s",
                                req.data.stream.poolIndex, req.data.stream.path);
                            break;
                        }
                        else {
                            Logger::Log(LogLevel::Error, "IoSystem", "Failed to open stream for: %s (error: %d)", req.data.stream.path, error);
                            stream.state.store(StreamState::Error, std::memory_order_release);
                        }

                        break;
                    }
                    case IoRequest::Type::ReleaseStream: {
                        StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];
                        if (stream.decoder) {
                            stb_vorbis_close(stream.decoder);
                            stream.decoder = nullptr;
                        }

                        stream.Reset();
                        stream.state = StreamState::Free;
                        m_freeStreams->Push(req.data.stream.poolIndex);
                        Logger::Log(LogLevel::Debug, "IoSystem", "Freed stream %d", req.data.stream.poolIndex);
                        break;
                    }
                    case IoRequest::Type::RefillStreamBuffer: {
                        StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];

                        if (stream.state.load(std::memory_order_acquire) != StreamState::Streaming) break;
                        if (stream.generation.load(std::memory_order_relaxed) != req.data.stream.generation) break;

                        FillBuffer(stream, req.data.stream.bufferIndex);
                        Logger::Log(LogLevel::Debug, "IoSystem", "Refilled buffer %d for stream %d",
                            req.data.stream.bufferIndex, req.data.stream.poolIndex);
                        break;
                    }
                    case IoRequest::Type::LoadSoundbank: {
                        // TODO: Logic for soundbank loading
                        break;
                    }
                    default:
                        break;
                }
            }

            if (!didWork) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Based on OS interrupt (Windows: 15.6ms)
            }
        }
    }

    void IoSystem::FillBuffer(StreamingContext& stream, uint32_t bufferIndex) {
        if (!stream.decoder) {
            Logger::Log(LogLevel::Error, "Stream buffer refill", "Invalid decoder");
            stream.state.store(StreamState::Error, std::memory_order_release);
            return;
        }

        float* bufferPtr = stream.buffers[bufferIndex];
        // int samplesNeeded = DOUBLE_BUFFER_FRAMES * stream.channels;
        // int samplesWritten = 0;
        // bool justLooped = false;

        int framesNeeded = dalia::DOUBLE_BUFFER_FRAMES;
        int framesWritten = 0;
        bool justLooped = false;


        while (framesWritten < framesNeeded) {
            int samplesRemaining = (framesNeeded - framesWritten) * stream.channels;
            float* writePtr = bufferPtr + (framesWritten * stream.channels);

            int framesRead = stb_vorbis_get_samples_float_interleaved(
                stream.decoder,
                stream.channels,
                writePtr,
                samplesRemaining
            );

            if (framesRead == 0) {
                // Hit EOF
                if (justLooped) {
                    Logger::Log(LogLevel::Error, "IoSystem", "Stream loop failed. Corrupted file.");
                    stream.state.store(StreamState::Error, std::memory_order_release);
                    return;
                }

                stream.eofIndex[bufferIndex] = framesWritten * stream.channels;
                stb_vorbis_seek_start(stream.decoder);
                justLooped = true;
                // We continue to fill buffer from the start of the file
            }
            else {
                justLooped = false;
                framesWritten += framesRead;
            }
        }

        stream.bufferReady[bufferIndex].store(true, std::memory_order_release);
    }
}
