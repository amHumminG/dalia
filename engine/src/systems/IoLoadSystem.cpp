#include "systems/IoLoadSystem.h"

#include "core/Logger.h"
#include "messaging/IoLoadRequestQueue.h"
#include "messaging/IoLoadEventQueue.h"

#include "stb_vorbis.c"

namespace dalia {

    IoLoadSystem::IoLoadSystem(const IoLoadSystemConfig& config)
        : m_ioLoadRequests(config.ioLoadRequests),
        m_ioLoadEvents(config.ioLoadEvents) {
    }

    IoLoadSystem::~IoLoadSystem() {
        Stop();
    }

    void IoLoadSystem::Start() {
        if (m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(true, std::memory_order_release);
        m_thread = std::thread(&IoLoadSystem::ThreadMain, this);
    }

    void IoLoadSystem::Stop() {
        if (!m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(false, std::memory_order_release);
        if (m_thread.joinable()) m_thread.join();
    }

    void IoLoadSystem::ThreadMain() {
        while (m_isRunning.load(std::memory_order_relaxed)) {
            bool didWork = false;

            // Loading logic
        }
    }
}
