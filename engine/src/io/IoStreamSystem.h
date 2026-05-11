#pragma once

#include <span>
#include <thread>

namespace dalia {

    class IoStreamRequestQueue;
    struct IoStreamRequest;
    struct StreamContext;
    template <typename T> class SPSCRingBuffer;

    struct IoStreamSystemConfig {
    	uint32_t outSampleRate;
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
        void ThreadMain();
        void ProcessRequest(const IoStreamRequest& req);
        void FillBuffer(StreamContext& stream, uint32_t bufferIndex);

    	uint32_t m_outSampleRate = 0;
        IoStreamRequestQueue* m_ioStreamRequests;
        std::span<StreamContext> m_streamPool;
        SPSCRingBuffer<uint32_t>* m_freeStreams;

        std::thread m_thread;
        std::atomic<bool> m_isRunning{false};
    };
}
