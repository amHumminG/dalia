#pragma once
#include <thread>
#include <atomic>

namespace dalia {

    class IoLoadRequestQueue;
    class IoLoadEventQueue;

    struct IoLoadSystemConfig {
        IoLoadRequestQueue* ioLoadRequests = nullptr;
        IoLoadEventQueue* ioLoadEvents = nullptr;
    };

    class IoLoadSystem {
    public:
        IoLoadSystem(const IoLoadSystemConfig& config);
        ~IoLoadSystem();

        void Start();
        void Stop();

    private:
        void ThreadMain();

        IoLoadRequestQueue* m_ioLoadRequests;
        IoLoadEventQueue* m_ioLoadEvents;

        std::thread m_thread;
        std::atomic<bool> m_isRunning;
    };
}