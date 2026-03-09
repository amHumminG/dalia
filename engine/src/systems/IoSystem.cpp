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

                        if (stream.decoder) {
                            stb_vorbis_close(stream.decoder);
                            stream.decoder = nullptr;
                        }

                        int error;
                        stream.decoder = stb_vorbis_open_filename(req.data.stream.path, &error, nullptr);

                        if (stream.decoder) {
                            stb_vorbis_info info = stb_vorbis_get_info(stream.decoder);
                            stream.channels = info.channels;
                            stream.sampleRate = info.sample_rate;
                        }
                        else {
                            // TODO: Invalid stream, maybe we set stream state to something like invalid so that audio thread knows its fucked? Or do we just release it here?
                            Logger::Log(LogLevel::Error, "IoSystem", "Failed to open stream for: %s (error: %d)", req.data.stream.path, error);
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
                        break;
                    }
                    case IoRequest::Type::RefillStreamBuffer: {
                        StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];

                        // Is the stream still valid?
                        if (stream.generation.load(std::memory_order_relaxed) != req.data.stream.generation) {
                            break;
                        }

                        if (!stream.decoder) {
                            // TODO: We need to signal to the audio thread that decoder is invalid here somehow!
                            Logger::Log(LogLevel::Error, "Stream buffer refill", "Invalid decoder");
                            break;
                        }

                        float* bufferPtr = stream.buffers[req.data.stream.bufferIndex];
                        int samplesNeeded = DOUBLE_BUFFER_FRAMES * stream.channels;
                        int samplesWritten = 0;

                        while (samplesWritten < samplesNeeded) {
                            int samplesRead = stb_vorbis_get_samples_float_interleaved(
                                stream.decoder,
                                stream.channels,
                                bufferPtr + samplesWritten,
                                samplesNeeded - samplesWritten
                            );

                            if (samplesRead == 0) {
                                // Hit EOF
                                stream.eofIndex[req.data.stream.bufferIndex] = samplesWritten;
                                stb_vorbis_seek_start(stream.decoder);
                                // We continue to fill buffer from the start of the file
                            }

                            samplesWritten += samplesRead;
                        }

                        stream.bufferReady[req.data.stream.bufferIndex].store(true, std::memory_order_release);
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Time based on windows interrupt
            }
        }
    }
}
