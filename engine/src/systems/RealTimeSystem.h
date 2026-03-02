#pragma once
#include <span>

namespace dalia {

    class RtCommandQueue;
    class RtEventQueue;
    class IoRequestQueue;
    struct Voice;
    struct StreamingContext;
    class Bus;

    struct RealTimeSystemConfig {
        RtCommandQueue* rtCommandQueue = nullptr;
        RtEventQueue*   rtEventQueue   = nullptr;
        IoRequestQueue* ioRequestQueue = nullptr;

        std::span<Voice>            voicePool;
        std::span<StreamingContext> streamPool;
        std::span<Bus>              busPool;
        Bus*                        masterBus = nullptr;
    };

    class RealTimeSystem {
    public:
        explicit RealTimeSystem(const RealTimeSystemConfig& config);
        void OnAudioCallback(float* output, uint32_t frameCount, uint32_t channels);

    private:
        void ProcessCommands();
        void Render(float* output, uint32_t frameCount, uint32_t channels);
        bool MixVoiceToBus(Voice& voice, uint32_t busIndex, uint32_t frameCount);

        std::span<Voice>            m_voicePool;
        std::span<StreamingContext> m_streamPool;
        std::span<Bus>              m_busPool;

        Bus*            m_masterBus;
        RtCommandQueue* m_rtCommandQueue;
        RtEventQueue*   m_rtEventQueue;
        IoRequestQueue* m_ioRequestQueue;

        std::span<const uint32_t> m_activeBusGraph;
    };
}