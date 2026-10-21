#pragma once

#include "messaging/AsyncStreamMessaging.h"

#include <span>
#include <thread>

namespace dalia {

	struct StreamContext;
    template <typename T> class SPSCRingBuffer;
	class MessageRouter;

    struct AsyncStreamSystemConfig {
    	uint32_t wakeupPeriodMicroseconds;
    	uint32_t outSampleRate;
        MessageRouter* asyncDirector = nullptr;
        std::span<StreamContext> streamPool;
        SPSCRingBuffer<uint32_t>* freeStreams = nullptr;
    };

    class AsyncStreamSystem {
    public:
        explicit AsyncStreamSystem(const AsyncStreamSystemConfig& config);
        ~AsyncStreamSystem();

        void Start();
        void Stop();

    private:
        void ThreadMain();
        void ProcessRequest(const AsyncStreamCommand& req);
        void FillBuffer(StreamContext& stream, uint32_t bufferIndex);

    	MessageRouter* m_asyncDirector = nullptr;
    	uint32_t m_outSampleRate = 0;
        std::span<StreamContext> m_streamPool;
        SPSCRingBuffer<uint32_t>* m_freeStreams;

        std::thread m_thread;
        std::atomic<bool> m_isRunning{false};
    	uint32_t m_wakeupPeriodMicroseconds = 0;
    };
}
