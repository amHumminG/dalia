#pragma once
#include <thread>
#include <atomic>

namespace dalia {

    class IoLoadRequestQueue;
    struct IoLoadRequest;
    class IoLoadEventQueue;
    class AssetRegistry;

    struct IoLoadSystemConfig {
        IoLoadRequestQueue* ioLoadRequests = nullptr;
        IoLoadEventQueue* ioLoadEvents = nullptr;

        AssetRegistry*  assetRegistry = nullptr;
    };

    class IoLoadSystem {
    public:
        IoLoadSystem(const IoLoadSystemConfig& config);
        ~IoLoadSystem();

        void Start();
        void Stop();

    private:
        void ThreadMain();
        void ProcessRequest(const IoLoadRequest& request);

        IoLoadRequestQueue* m_ioLoadRequests;
        IoLoadEventQueue* m_ioLoadEvents;

        AssetRegistry* m_assetRegistry;

        std::thread m_thread;
        std::atomic<bool> m_isRunning;
    };
}