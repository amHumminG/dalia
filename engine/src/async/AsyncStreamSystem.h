#pragma once

#include "messaging/AsyncStreamMessaging.h"

#include <span>
#include <thread>

namespace dalia {

	struct StreamContext;
	class AsyncMessageBus;

    struct AsyncStreamSystemConfig {
    	AsyncMessageBus* messageRouter = nullptr;
    	uint32_t wakeupPeriodMicroseconds;

        std::span<StreamContext> streamPool;
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

    	std::thread m_thread;
    	std::atomic<bool> m_isRunning{false};
    	uint32_t m_wakeupPeriodMicroseconds = 0;

    	AsyncMessageBus* m_messageRouter = nullptr;

        std::span<StreamContext> m_streamPool;
    };
}
