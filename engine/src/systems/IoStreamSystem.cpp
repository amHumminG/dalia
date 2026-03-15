#include "systems/IoStreamSystem.h"

#include "core/Logger.h"

#include "core/SPSCRingBuffer.h"
#include "messaging/IoStreamRequestQueue.h"
#include "mixer/StreamContext.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include "core/Utility.h"

#include <chrono>

namespace dalia {

    IoStreamSystem::IoStreamSystem(const IoStreamSystemConfig& config)
        : m_ioStreamRequests(config.ioStreamRequests),
        m_streamPool(config.streamPool),
        m_freeStreams(config.freeStreams) {
    }

    IoStreamSystem::~IoStreamSystem() {
        Stop();
    }

    void IoStreamSystem::Start() {
        if (m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(true, std::memory_order_release);
        m_thread = std::thread(&IoStreamSystem::ThreadMain, this);
    }

    void IoStreamSystem::Stop() {
        if (!m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(false, std::memory_order_release);
        if (m_thread.joinable()) m_thread.join();
    }

    void IoStreamSystem::ThreadMain() {
        while (m_isRunning.load(std::memory_order_relaxed)) {
            bool didWork = false;
            IoStreamRequest req;

            while (m_ioStreamRequests->Pop(req)) {
                didWork = true;

                // Handle requests
                switch (req.type) {
                    case IoStreamRequest::Type::PrepareStream: {
                        StreamContext& stream = m_streamPool[req.data.stream.poolIndex];
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
                                Logger::Log(LogLevel::Error, "IoStreamSystem",
                                    "Failed to load file: %s. Unsupported channel count (%d) in file.",
                                    req.data.stream.path, info.channels);
                                isSupported = false;
                            }

                            // Sample rate
                            static constexpr uint32_t ENGINE_SAMPLE_RATE = 48000;
                            if (info.sample_rate != ENGINE_SAMPLE_RATE) {
                                Logger::Log(LogLevel::Error, "IoStreamSystem",
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

                            Logger::Log(LogLevel::Debug, "IoStreamSystem", "Finished preparing stream %d for file: %s",
                                req.data.stream.poolIndex, req.data.stream.path);
                            break;
                        }
                        else {
                            Logger::Log(LogLevel::Error, "IoStreamSystem", "Failed to open stream for %s. %s.",
                                req.data.stream.path, GetStbVorbisErrorString(error));
                            stream.state.store(StreamState::Error, std::memory_order_release);
                        }

                        break;
                    }
                    case IoStreamRequest::Type::ReleaseStream: {
                        StreamContext& stream = m_streamPool[req.data.stream.poolIndex];
                        if (stream.decoder) {
                            stb_vorbis_close(stream.decoder);
                            stream.decoder = nullptr;
                        }

                        stream.Reset();
                        stream.state = StreamState::Free;
                        m_freeStreams->Push(req.data.stream.poolIndex);
                        Logger::Log(LogLevel::Debug, "IoStreamSystem", "Freed stream %d", req.data.stream.poolIndex);
                        break;
                    }
                    case IoStreamRequest::Type::RefillStreamBuffer: {
                        StreamContext& stream = m_streamPool[req.data.stream.poolIndex];

                        if (stream.state.load(std::memory_order_acquire) != StreamState::Streaming) break;
                        if (stream.generation.load(std::memory_order_relaxed) != req.data.stream.generation) break;

                        FillBuffer(stream, req.data.stream.bufferIndex);
                        Logger::Log(LogLevel::Debug, "IoStreamSystem", "Refilled buffer %d for stream %d",
                            req.data.stream.bufferIndex, req.data.stream.poolIndex);
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

    void IoStreamSystem::FillBuffer(StreamContext& stream, uint32_t bufferIndex) {
        if (!stream.decoder) {
            Logger::Log(LogLevel::Error, "Stream buffer refill", "Invalid decoder");
            stream.state.store(StreamState::Error, std::memory_order_release);
            return;
        }

        float* bufferPtr = stream.buffers[bufferIndex];

        int framesNeeded = dalia::DOUBLE_BUFFER_FRAMES;
        int framesWritten = 0;
        bool justLooped = false;
        bool foundEOF = false;


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
                    Logger::Log(LogLevel::Error, "IoStreamSystem", "Stream loop failed. Corrupted file.");
                    stream.state.store(StreamState::Error, std::memory_order_release);
                    return;
                }

                stream.eofIndex[bufferIndex] = framesWritten;
                Logger::Log(LogLevel::Debug, "IoStreamSystem", "Stream buffer found EOF at index %d", framesWritten);
                stb_vorbis_seek_start(stream.decoder);
                justLooped = true;
                foundEOF = true;
                // We continue to fill buffer from the start of the file
            }
            else {
                if (!foundEOF) stream.eofIndex[bufferIndex] = NO_EOF;
                framesWritten += framesRead;
                justLooped = false;
            }
        }

        stream.bufferReady[bufferIndex].store(true, std::memory_order_release);
    }
}
