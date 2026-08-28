#pragma once
#include <thread>
#include <atomic>

namespace dalia {

    class IoLoadRequestQueue;
    struct IoLoadRequest;
    class IoLoadEventQueue;
    class AssetRegistry;

    struct AsyncLoadSystemConfig {
    	uint32_t outSampleRate = 0;

        IoLoadRequestQueue* ioLoadRequests = nullptr;
        IoLoadEventQueue* ioLoadEvents = nullptr;

        AssetRegistry*  assetRegistry = nullptr;
    };

    class AsyncLoadSystem {
    public:
        AsyncLoadSystem(const AsyncLoadSystemConfig& config);
        ~AsyncLoadSystem();

        void Start();
        void Stop();

    private:
        void ThreadMain();
        void ProcessRequest(const IoLoadRequest& request);

    	uint32_t m_outSampleRate = 0;

        IoLoadRequestQueue* m_ioLoadRequests;
        IoLoadEventQueue* m_ioLoadEvents;

        AssetRegistry* m_assetRegistry;

        std::thread m_thread;
        std::atomic<bool> m_isRunning;
    };
}