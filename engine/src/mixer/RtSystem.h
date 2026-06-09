#pragma once

#include "core/Constants.h"
#include "core/ParameterBridge.h"
#include "mixer/Speakers.h"
#include "mixer/PeakLimiter.h"

#include <span>

namespace dalia {

    class RtCommandQueue;
    class RtEventQueue;
    class IoStreamRequestQueue;

	struct Listener;
	struct ListenerParams;
	struct StreamContext;
    struct Voice;
	struct VoiceParams;
    struct Bus;
	struct BusParams;
	struct Biquad;
	struct BiquadParams;
    struct EffectHandle;
    struct EffectSlot;

	enum class CoordinateSystem : uint8_t;
	enum class SpeakerLayout : uint8_t;
	struct VirtualSpeaker;

	class MixGraphCompiler;

    struct RtSystemConfig {
    	CoordinateSystem coordinateSystem;
    	float globalDopplerFactor = 1.0f;

        SpeakerLayout speakerLayout;
    	uint32_t maxSamplesPerPeriod = 0;
        uint32_t outChannels = 0;
        uint32_t outSampleRate = 0;

        RtCommandQueue* rtCommands              = nullptr;
        RtEventQueue* rtEvents                  = nullptr;
        IoStreamRequestQueue* ioStreamRequests  = nullptr;

    	std::span<StreamContext> streamPool;

        std::span<Voice> voicePool;
    	std::span<ParameterBridge<VoiceParams>> voiceParamBridges;

    	std::span<Listener> listenerPool;
    	std::span<ParameterBridge<ListenerParams>> listenerParamBridges;

        std::span<Bus> busPool;
    	std::span<ParameterBridge<BusParams>> busParamBridges;
        std::span<float> busBufferPool;

    	std::span<Biquad> biquadPool;
    	std::span<ParameterBridge<BiquadParams>> biquadParamBridges;

    	MixGraphCompiler* mixGraphCompiler		= nullptr;
    	std::span<uint32_t> mixOrder;
        std::span<float> dspScratchBuffer;
    };

    class RtSystem {
    public:
        explicit RtSystem(const RtSystemConfig& config);
        void Tick(float* output, uint32_t frameCount);

    private:
        void ProcessCommands();
    	void ProcessParams();
        void Render(float* output, uint32_t frameCount);

    	void ResolveVoiceStates();
    	void ResolveVoiceAcoustics();
        uint32_t RenderVoices(uint32_t frameCount);
        void FreeVoice(uint32_t voiceIndex);

    	bool ResolveBusState(Bus& bus);
        void RenderBus(uint32_t busIndex, uint32_t frameCount);
        void ApplyBusEffect(float* buffer, EffectSlot& slot, uint32_t frameCount);

        void AttachEffect(EffectHandle handle, uint32_t busIndex, uint32_t effectSlot);
        void DetachEffect(EffectHandle handle, uint32_t busIndex, uint32_t effectSlot);
        void FadeOutEffect(EffectHandle handle, uint32_t busIndex, uint32_t effectSlot);

    	void ConfigureSpeakerLayout(SpeakerLayout layout); // Returns the spatial speaker count

    	PeakLimiter m_masterPeakLimiter;
    	float m_smoothingCoefficient = 0.0f; // Used for volume and gain smoothing
    	float m_fadeStep = 0.0f;			 // Per sample step for gain fading

    	// Global User Settings
    	CoordinateSystem m_coordinateSystem;
    	float m_globalDopplerFactor = 1.0f;

    	// Output Settings
    	SpeakerLayout m_speakerLayout;
    	VirtualSpeaker m_speakerMatrix[CHANNELS_MAX];
    	uint32_t m_spatialSpeakerCount = 0;
    	uint32_t m_maxSamplesPerPeriod = 0;
        uint32_t m_outChannels = 0;
        uint32_t m_outSampleRate = 0;

    	// Messaging
        RtCommandQueue* m_rtCommands				= nullptr;
        RtEventQueue* m_rtEvents					= nullptr;
        IoStreamRequestQueue* m_ioStreamRequests	= nullptr;

    	// Streams
    	std::span<StreamContext> m_streamPool;

    	// Voices
        std::span<Voice> m_voicePool;
    	std::span<ParameterBridge<VoiceParams>> m_voiceParamBridges;

    	// Listeners
    	std::span<Listener> m_listenerPool;
    	std::span<ParameterBridge<ListenerParams>> m_listenerParamBridges;

    	// Buses
        std::span<Bus> m_busPool;
    	std::span<ParameterBridge<BusParams>> m_busParamBridges;
        std::span<float> m_busBufferPool;

    	// Effects
    	std::span<Biquad> m_biquadPool;
    	std::span<ParameterBridge<BiquadParams>> m_biquadParamBridges;

    	// Mixing Graph & DSP
		MixGraphCompiler* m_mixGraphCompiler = nullptr;
		std::span<uint32_t>	m_mixOrder;
    	uint32_t m_mixOrderSize = 0;
    	bool m_isMixOrderDirty = true;

    	std::span<float> m_dspScratchBuffer;
    };
}
