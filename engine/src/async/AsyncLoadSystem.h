#pragma once

#include "messaging/AsyncLoadMessaging.h"

#include <thread>
#include <atomic>
#include <semaphore>

namespace dalia {

    class AssetRegistry;
	class MessageRouter;

    struct AsyncLoadSystemConfig {
    	uint32_t outSampleRate = 0;

        MessageRouter* messageRouter = nullptr;
        AssetRegistry*  assetRegistry = nullptr;
    };

    class AsyncLoadSystem {
    public:
        AsyncLoadSystem(const AsyncLoadSystemConfig& config);
        ~AsyncLoadSystem();

        void Start();
        void Stop();

    	void NotifyTaskAdded();

    private:
        void ThreadMain();
    	void LoadSound(const AsyncLoadCommand& cmd);

    	std::thread m_thread;
    	std::atomic<bool> m_isRunning;
    	std::counting_semaphore<COUNTING_SEMAPHORE_SIZE> m_taskSemaphore{0};

    	MessageRouter* m_messageRouter = nullptr;

    	// TODO: See if we can send these via a command maybe?
    	AssetRegistry* m_assetRegistry = nullptr;
    	uint32_t m_outSampleRate = 0;
    };
}