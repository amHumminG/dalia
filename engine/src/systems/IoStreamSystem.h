#pragma once

#include <span>
#include <thread>

namespace dalia {

    class IoStreamRequestQueue;
    struct StreamContext;
    template <typename T> class SPSCRingBuffer;

    struct IoStreamSystemConfig {
        IoStreamRequestQueue* ioStreamRequests = nullptr;
        std::span<StreamContext> streamPool;
        SPSCRingBuffer<uint32_t>*   freeStreams = nullptr;
    };

    class IoStreamSystem {
    public:
        explicit IoStreamSystem(const IoStreamSystemConfig& config);
        ~IoStreamSystem();

        void Start();
        void Stop();

    private:
        void IoThreadMain();

        void FillBuffer(StreamContext& stream, uint32_t bufferIndex);

        IoStreamRequestQueue* m_ioStreamRequests;
        std::span<StreamContext> m_streamPool;
        SPSCRingBuffer<uint32_t>* m_freeStreams;

        std::thread m_ioThread;
        std::atomic<bool> m_isRunning{false};
    };
}
