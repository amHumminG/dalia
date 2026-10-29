#pragma once

#include "dalia/PlaybackControl.h"
#include "dalia/Result.h"

#include "core/Constants.h"
#include "core/SPSCRingBufferPCM.h"
#include "core/ParameterBridge.h"
#include "core/StateSyncPool.h"

#include "async/WakeableThread.h"

#include "mixer/Speakers.h"
#include "dsp/PeakLimiter.h"
#include "mixer/MixGraphCompiler.h"

namespace dalia {

	class AsyncMessageBus;
	struct AsyncMixerCommand;
	struct StreamContext;
	struct Voice;
	struct VoiceParams;
	struct Listener;
	struct ListenerParams;
	struct Bus;
	struct BusParams;
	struct Biquad;
	struct BiquadParams;
	struct EffectHandle;
	struct EffectSlot;

	struct MixerConfig {
		CoordinateSystem coordinateSystem = CoordinateSystem::LeftHanded;

		SpeakerLayout speakerLayout = SpeakerLayout::Mono;
		uint32_t maxSamplesPeriod = 0;
		uint32_t channels = CHANNELS_MONO;
		uint32_t sampleRate = 48000;
		uint32_t periodSizeFrames = 512;

		std::span<StreamContext> streamPool;
		MixerPoolView<Voice, VoiceParams> voicePool;
		MixerPoolView<Listener, ListenerParams> listenerPool;
		MixerPoolView<Bus, BusParams> busPool;
		MixerPoolView<Biquad, BiquadParams> biquadPool;
	};

	class Mixer {
	public:
		using WakeupCallback = std::function<void()>;

		Mixer();
		~Mixer();

		Result Initialize(AsyncMessageBus* asyncMessageBus, WakeableThread* mixerThread, const MixerConfig& config);
		void Shutdown();

		void ProcessAudioPeriod();

		// === OutputDevice / DeviceController Interface ===
		size_t ReadDiscardAudio(float* buffer, uint32_t frameCount);
		size_t DiscardAudio(uint32_t frameCount);
		void Wake();

		void SetOutputFormat(uint32_t channels, SpeakerLayout layout);

	private:
		void SubscribeToCommands();
		void UnsubscribeFromCommands();

		// Command handling
		void HandleAllocateVoiceCommand(const AsyncMixerCommand& cmd);
		void HandleFreeVoiceCommand(const AsyncMixerCommand& cmd);
		void HandlePrepareVoiceResidentCommand(const AsyncMixerCommand& cmd);
		void HandlePrepareVoiceStreamingCommand(const AsyncMixerCommand& cmd);
		void HandleSeekVoiceCommand(const AsyncMixerCommand& cmd);
		void HandlePlayVoiceCommand(const AsyncMixerCommand& cmd);
		void HandlePauseVoiceCommand(const AsyncMixerCommand& cmd);
		void HandleStopVoiceCommand(const AsyncMixerCommand& cmd);
		void HandleSetVoiceParentCommand(const AsyncMixerCommand& cmd);

		void HandleAllocateBusCommand(const AsyncMixerCommand& cmd);
		void HandleFreeBusCommand(const AsyncMixerCommand& cmd);
		void HandleSetBusParentCommand(const AsyncMixerCommand& cmd);

		void HandleAllocateEffectCommand(const AsyncMixerCommand& cmd);
		void HandleFreeEffectCommand(const AsyncMixerCommand& cmd);
		void HandleAttachEffectCommand(const AsyncMixerCommand& cmd);
		void HandleFadeDetachEffectCommand(const AsyncMixerCommand& cmd);
		void HandleForceDetachEffectCommand(const AsyncMixerCommand& cmd);

		void HandleSetGlobalDopplerFactorCommand(const AsyncMixerCommand& cmd);

		// DSP Pipeline
		void SyncParams();
		void Render(float* output, uint32_t frameCount);
		bool CompileMixGraph();

		void ResolveVoiceStates();
		void ResolveVoiceAcoustics();
		uint32_t RenderVoices(uint32_t frameCount);
		void FreeVoice(uint32_t voiceIndex);

		bool ResolveBusStates();
		uint32_t RenderBuses(uint32_t frameCount);
		void ApplyBusEffect(float* buffer, EffectSlot& slot, uint32_t frameCount);

		void ConfigureSpeakerLayout(SpeakerLayout layout);

		// --- Utility ---
		[[nodiscard]] float* GetBusBuffer(uint32_t busIndex) const;

		// --- Synchronization ---
		std::atomic<bool> m_isProcessing{false};
		std::atomic<bool> m_suspendRequest{false};

		AsyncMessageBus* m_asyncMessageBus	= nullptr;
		WakeableThread*  m_mixerThread		= nullptr;

		// --- Output ---
		uint32_t m_periodSizeFrames		= 512;
		std::unique_ptr<SPSCRingBufferPCM<float>> m_pcmRingBuffer;
		PeakLimiter m_masterPeakLimiter;
		float m_smoothingCoefficient	= 0.0f;
		float m_fadeStep				= 0.0f;
		float m_outputFadeGain			= 0.0f;

		// Configuration
		CoordinateSystem m_coordinateSystem = CoordinateSystem::LeftHanded;
		float m_globalDopplerFactor = DOPPLER_FACTOR_DEFAULT;

		SpeakerLayout m_speakerLayout	= SpeakerLayout::Mono;
		VirtualSpeaker m_speakerMatrix[CHANNELS_MAX]{};
		uint32_t m_spatialSpeakerCount	= 0;
		uint32_t m_maxSamplesPeriod		= 0;
		uint32_t m_outChannels			= 0;
		uint32_t m_outSampleRate		= 0;

		std::span<StreamContext>					m_streamPool;
		MixerPoolView<Voice, VoiceParams>			m_voicePool;
		MixerPoolView<Listener, ListenerParams>		m_listenerPool;
		MixerPoolView<Bus, BusParams>				m_busPool;
		MixerPoolView<Biquad, BiquadParams>			m_biquadPool;

		// --- DSP Working Memory ---
		std::unique_ptr<MixGraphCompiler>	m_mixGraphCompiler;
		std::unique_ptr<uint32_t[]>			m_mixOrder;
		uint32_t							m_mixOrderSize		= 0;
		bool								m_isMixOrderDirty	= true;

		std::unique_ptr<float[]>			m_busBufferPool;
		std::unique_ptr<float[]>			m_mixScratchBuffer;
	};
}
