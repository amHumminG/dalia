#include "systems/IoSystem.h"

#include "core/SPSCRingBuffer.h"
#include "messaging/IoRequestQueue.h"
#include "mixer/StreamingContext.h"

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
                case IoRequest::Type::RefillStreamBuffer: {
                    StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];

                    // Is the stream still valid?
                    if (stream.generation.load(std::memory_order_relaxed) != req.data.stream.generation) {
                        break;
                    }

                    // TODO: Read data into buffer from soundbank
                    // Remember that if read reaches EOF, it should mark it in the buffer and keep reading
                    // from the start of the file to fill the entire buffer.
                }
                case IoRequest::Type::ReleaseStream: {
                    StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];
                    // TODO: Release decoder here (if StreamingContext has one)
                    m_freeStreams->Push(req.data.stream.poolIndex);
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Time based on windows interrupt
            }
        }
    }
}
