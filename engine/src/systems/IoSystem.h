#pragma once

#include <span>
#include <thread>

#include "../core/SPSCRingBuffer.h"

namespace dalia {

    class IoRequestQueue;
    struct StreamingContext;
    template <typename T> class SPSCRingBuffer;

    struct IoSystemConfig {
        IoRequestQueue* ioRequestQueue = nullptr;

        std::span<StreamingContext> streamPool;
        SPSCRingBuffer<uint32_t>*   freeStreams = nullptr;
    };

    class IoSystem {
    public:
        explicit IoSystem(const IoSystemConfig& config);
        ~IoSystem();

        void Start();
        void Stop();

    private:
        void IoThreadMain();

        void FillBuffer(StreamingContext& stream, uint32_t bufferIndex);

        IoRequestQueue* m_ioRequestQueue;
        std::span<StreamingContext> m_streamPool;
        SPSCRingBuffer<uint32_t>* m_freeStreams;

        std::thread m_ioThread;
        std::atomic<bool> m_isRunning{false};
    };
}
