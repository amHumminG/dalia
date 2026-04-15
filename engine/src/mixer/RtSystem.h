#pragma once

#include "core/Constants.h"
#include "mixer/ParameterBridge.h"
#include "mixer/Speakers.h"

#include <span>
#include <memory>

namespace dalia {

    class RtCommandQueue;
    class RtEventQueue;
    class IoStreamRequestQueue;

	// template <typename T> struct ParameterBridge;
    struct Voice;
	struct VoiceParams;
    struct StreamContext;
    struct Bus;
    class EffectHandle;
    struct EffectSlot;
	struct Listener;
	struct ListenerParams;

	enum class SpeakerLayout;
	struct VirtualSpeaker;

	class MixGraphCompiler;

    enum class EffectType : uint8_t;
    struct BiquadFilter;

    struct RtSystemConfig {
        SpeakerLayout speakerLayout;
        uint32_t outChannels = 0;
        uint32_t outSampleRate = 0;

        RtCommandQueue* rtCommands              = nullptr;
        RtEventQueue* rtEvents                  = nullptr;
        IoStreamRequestQueue* ioStreamRequests  = nullptr;

        std::span<Voice> voicePool;
    	std::span<ParameterBridge<VoiceParams>> voiceParamBridges;
        std::span<StreamContext> streamPool;
        std::span<Bus> busPool;
        std::span<float> busBufferPool;

    	MixGraphCompiler* mixGraphCompiler		= nullptr;
    	std::span<uint32_t> mixOrder;

    	std::span<Listener> listenerPool;
    	std::span<ParameterBridge<ListenerParams>> listenerParamBridges;

        std::span<float> dspScratchBuffer;
        std::span<BiquadFilter> biquadFilterPool;
    };

    class RtSystem {
    public:
        explicit RtSystem(const RtSystemConfig& config);
        void Tick(float* output, uint32_t frameCount);

    private:
        void ProcessCommands();
    	void ProcessParams();
        void Render(float* output, uint32_t frameCount);
    	void ResolveVoice(Voice& voice);
        bool ProcessVoice(uint32_t voiceIndex, uint32_t frameCount);
    	void EvaluateVoiceTargetGains();
        void FreeVoice(uint32_t voiceIndex);
    	bool ResolveBus(Bus& bus);
        void ProcessBus(uint32_t busIndex, uint32_t frameCount);
        void ApplyBusEffect(float* busBuffer, EffectSlot& slot, uint32_t frameCount);
        void AttachEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot);
        void DetachEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot);
        void FadeOutEffect(EffectHandle effect, uint32_t busIndex, uint32_t effectSlot);
        void FlushEffect(EffectType type, uint32_t index, uint32_t gen);
    	void ConfigureSpeakerLayout(SpeakerLayout layout); // Returns the spatial speaker count

    	SpeakerLayout m_speakerLayout;
    	uint32_t m_spatialSpeakerCount = 0;
        uint32_t m_outChannels = 0;
        uint32_t m_outSampleRate = 0;

    	VirtualSpeaker m_speakerMatrix[CHANNELS_MAX];

        float m_smoothingCoefficient = 0.0f; // Used for volume and gain smoothing
    	float m_fadeStep = 0.0f; // Per sample step for gain fading

        RtCommandQueue* m_rtCommands				= nullptr;
        RtEventQueue* m_rtEvents					= nullptr;
        IoStreamRequestQueue* m_ioStreamRequests	= nullptr;

        std::span<Voice> m_voicePool;
    	std::span<ParameterBridge<VoiceParams>> m_voiceParamBridges;
        std::span<StreamContext> m_streamPool;
        std::span<Bus> m_busPool;
        std::span<float> m_busBufferPool;

        std::span<float> m_dspScratchBuffer;
        std::span<BiquadFilter> m_biquadFilterPool;

		MixGraphCompiler* m_mixGraphCompiler = nullptr;
		std::span<uint32_t>	m_mixOrder;

    	std::span<Listener> m_listenerPool;
    	std::span<ParameterBridge<ListenerParams>> m_listenerParamBridges;

    	uint32_t m_mixOrderSize = 0;
    	bool m_isMixOrderDirty = true;
    };
}
