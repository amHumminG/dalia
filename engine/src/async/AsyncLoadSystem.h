#pragma once

#include "messaging/AsyncLoadMessaging.h"

#include <thread>
#include <atomic>
#include <semaphore>

namespace dalia {

    class AssetRegistry;

    struct AsyncLoadSystemConfig {
    	uint32_t outSampleRate = 0;

        AsyncLoadRequestQueue* ioLoadRequests = nullptr;
        AsyncLoadEventQueue* ioLoadEvents = nullptr;

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
        void ProcessRequest(const AsyncLoadRequest& request);

    	uint32_t m_outSampleRate = 0;

        AsyncLoadRequestQueue* m_ioLoadRequests;
        AsyncLoadEventQueue* m_ioLoadEvents;

        AssetRegistry* m_assetRegistry;

        std::thread m_thread;
        std::atomic<bool> m_isRunning;
    	std::counting_semaphore<COUNTING_SEMAPHORE_SIZE> m_taskSemaphore{0};
    };
}