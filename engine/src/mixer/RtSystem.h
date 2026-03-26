#pragma once
#include <span>

namespace dalia {

    class RtCommandQueue;
    class RtEventQueue;
    class IoStreamRequestQueue;

    struct Voice;
    struct StreamContext;
    struct Bus;

    enum class EffectType : uint8_t;
    struct Biquad;

    struct RtSystemConfig {
        // Maybe we should just inject the device pointer itself
        uint32_t outputChannels = 0;
        uint32_t outputSampleRate = 0;

        RtCommandQueue* rtCommands              = nullptr;
        RtEventQueue* rtEvents                  = nullptr;
        IoStreamRequestQueue* ioStreamRequests  = nullptr;

        std::span<Voice> voicePool;
        std::span<StreamContext> streamPool;
        std::span<Bus> busPool;
        std::span<float> busBufferPool;
        Bus* masterBus                          = nullptr;

        std::span<Biquad> biquadPool;
    };

    class RtSystem {
    public:
        explicit RtSystem(const RtSystemConfig& config);
        void OnAudioCallback(float* output, uint32_t frameCount, uint32_t channels);

    private:
        void ProcessCommands();
        void Render(float* output, uint32_t frameCount, uint32_t channels);
        bool ProcessVoice(uint32_t voiceIndex, uint32_t frameCount, uint32_t channels);
        void FreeVoice(uint32_t voiceIndex);
        void ProcessBus(uint32_t busIndex, uint32_t frameCount, uint32_t channels);
        void ApplyEffect(float* buffer, uint32_t frameCount, uint32_t channels, EffectType type, uint32_t effectIndex);

        uint32_t m_outputChannels = 0; // Not used right now
        uint32_t m_outputSampleRate = 0;

        RtCommandQueue* m_rtCommands;
        RtEventQueue* m_rtEvents;
        IoStreamRequestQueue* m_ioStreamRequests;

        std::span<Voice> m_voicePool;
        std::span<StreamContext> m_streamPool;
        std::span<Bus> m_busPool;
        std::span<float> m_busBufferPool;
        Bus* m_masterBus;

        std::span<Biquad> m_biquadPool;

        std::span<const uint32_t> m_activeMixOrder;
    };
}