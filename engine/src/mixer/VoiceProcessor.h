#pragma once

#include "core/StateSyncPool.h"
#include "mixer/nodes/Voice.h"
#include "mixer/nodes/StreamContext.h"

#include <cstdint>
#include <memory>
#include <span>

namespace dalia {

	class AsyncMessageBus;
	class BusProcessor;
	struct AsyncMixerCommand;

	struct VoiceProcessorConfig {
		MixerPoolView<Voice, VoiceParams> voicePool;
		std::span<StreamContext> streamPool;
		uint32_t maxSamplesPeriod = 0;
		uint32_t outChannels = CHANNELS_MONO;
		uint32_t outSampleRate = 48000;
		float smoothingCoefficient = 0.0f;
		float fadeStep = 0.0f;
	};

	class VoiceProcessor {
	public:
		VoiceProcessor() = default;
		~VoiceProcessor();

		void Initialize(
			AsyncMessageBus* asyncMessageBus,
			BusProcessor* busProcessor,
			const VoiceProcessorConfig& config
		);
		void Shutdown();

		void SyncNodes() { m_voicePool.SyncParams(); };
		void SetOutputChannels(uint32_t outChannels) { m_outChannels = outChannels; }

		[[nodiscard]] std::span<Voice> GetVoices() const { return m_voicePool.nodes; };

		void ResolveStates();

		uint32_t RenderVoices(uint32_t frameCount);

	private:
		struct SourceBlock {
			const float* pcmData = nullptr;
			uint32_t framesAvailable = 0;
			uint32_t channels = 0;
			bool isError = false;
			bool isUnderrun = false;
		};

		void SubscribeToCommands();
		void UnsubscribeFromCommands();

		// --- Command Handlers ---
		void HandleAllocateVoiceCommand(const AsyncMixerCommand& cmd);
		void HandleFreeVoiceCommand(const AsyncMixerCommand& cmd);
		void HandlePrepareVoiceResidentCommand(const AsyncMixerCommand& cmd);
		void HandlePrepareVoiceStreamingCommand(const AsyncMixerCommand& cmd);
		void HandleSeekVoiceCommand(const AsyncMixerCommand& cmd);
		void HandlePlayVoiceCommand(const AsyncMixerCommand& cmd);
		void HandlePauseVoiceCommand(const AsyncMixerCommand& cmd);
		void HandleStopVoiceCommand(const AsyncMixerCommand& cmd);
		void HandleSetVoiceParentCommand(const AsyncMixerCommand& cmd);

		// --- State Resolution Helpers ---
		static bool ShouldFreeVoice(Voice& voice);
		static bool RequiresSilentTransition(const Voice& voice);
		void ApplyTransitions(Voice& voice);
		void ExecutePendingSeek(Voice& voice);
		void FreeVoice(uint32_t voiceIndex);

		// --- Rendering Helpers ---
		void RenderVoice(uint32_t voiceIndex, Voice& voice, uint32_t frameCount);
		[[nodiscard]] SourceBlock FetchSourceBlock(const Voice& voice, uint32_t maxFramesNeeded) const;
		bool AdvanceVoiceBoundary(Voice& voice);

		AsyncMessageBus*	m_asyncMessageBus	= nullptr;
		BusProcessor*		m_busProcessor		= nullptr;

		MixerPoolView<Voice, VoiceParams>	m_voicePool;
		std::span<StreamContext>			m_streamPool;

		uint32_t m_maxSamplesPeriod = 0;
		uint32_t m_outChannels = 0;
		uint32_t m_outSampleRate = 0;
		float m_smoothingCoefficient = 0.0f;
		float m_fadeStep = 0.0f;

		std::unique_ptr<float[]> m_scratchBuffer;
	};
}
