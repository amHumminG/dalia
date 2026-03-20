#pragma once
#include <span>

namespace dalia {

    class RtCommandQueue;
    class RtEventQueue;
    class IoStreamRequestQueue;
    struct Voice;
    struct StreamContext;
    class Bus;

    struct RtSystemConfig {
        RtCommandQueue* rtCommands              = nullptr;
        RtEventQueue* rtEvents                  = nullptr;
        IoStreamRequestQueue* ioStreamRequests  = nullptr;

        std::span<Voice> voicePool;
        std::span<StreamContext> streamPool;
        std::span<Bus> busPool;
        Bus* masterBus                          = nullptr;
    };

    class RtSystem {
    public:
        explicit RtSystem(const RtSystemConfig& config);
        void OnAudioCallback(float* output, uint32_t frameCount, uint32_t channels);

    private:
        void ProcessCommands();
        void Render(float* output, uint32_t frameCount, uint32_t channels);
        bool MixVoiceToBus(Voice& voice, uint32_t busIndex, uint32_t frameCount);
        void FreeVoice(uint32_t voiceIndex);

        RtCommandQueue* m_rtCommands;
        RtEventQueue* m_rtEvents;
        IoStreamRequestQueue* m_ioStreamRequests;

        std::span<Voice> m_voicePool;
        std::span<StreamContext> m_streamPool;
        std::span<Bus> m_busPool;
        Bus* m_masterBus;

        std::span<const uint32_t> m_activeMixOrder;
    };
}