#pragma once
#include <span>

namespace dalia {

    class RtCommandQueue;
    class RtEventQueue;
    class IoStreamRequestQueue;

    struct Voice;
    struct StreamContext;
    struct Bus;
    class EffectHandle;
    struct EffectSlot;

    enum class EffectType : uint8_t;
    struct BiquadFilter;

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

        std::span<float> dspScratchBuffer;
        std::span<BiquadFilter> biquadFilterPool;
    };

    class RtSystem {
    public:
        explicit RtSystem(const RtSystemConfig& config);
        void OnAudioCallback(float* output, uint32_t frameCount);

    private:
        void ProcessCommands();
        void Render(float* output, uint32_t frameCount);
        bool ProcessVoice(uint32_t voiceIndex, uint32_t frameCount);
    	void ReconcileVoice(Voice& voice);
        void FreeVoice(uint32_t voiceIndex);
        void ProcessBus(uint32_t busIndex, uint32_t frameCount);
        void ApplyBusEffect(float* busBuffer, EffectSlot& slot, uint32_t frameCount);
        void AttachEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot);
        void DetachEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot);
        void FadeOutEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot);
        void FlushEffect(EffectType type, uint32_t index, uint32_t gen);

        uint32_t m_outputChannels = 0;
        uint32_t m_outputSampleRate = 0;

        float m_smoothingCoefficient = 0.0f; // Used for volume and gain smoothing
    	float m_fadeStep = 0.0f; // Per sample step for gain fading

        RtCommandQueue* m_rtCommands;
        RtEventQueue* m_rtEvents;
        IoStreamRequestQueue* m_ioStreamRequests;

        std::span<Voice> m_voicePool;
        std::span<StreamContext> m_streamPool;
        std::span<Bus> m_busPool;
        std::span<float> m_busBufferPool;
        Bus* m_masterBus;

        std::span<float> m_dspScratchBuffer;
        std::span<BiquadFilter> m_biquadFilterPool;

        std::span<const uint32_t> m_activeMixOrder;
    };
}