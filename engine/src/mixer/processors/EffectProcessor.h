#pragma once

#include "dalia/EffectControl.h"
#include "core/StateSyncPool.h"
#include "mixer/nodes/Bus.h"
#include "mixer/nodes/effects/Biquad.h"

namespace dalia {

	class AsyncMessageBus;
	struct AsyncMixerCommand;

	struct EffectProcessorConfig {
		MixerPoolView<Biquad, BiquadParams> biquadPool;
		uint32_t maxSamplesPeriod = 0;
		uint32_t outChannels = CHANNELS_MONO;
		uint32_t outSampleRate = 48000;
	};

	class EffectProcessor {
	public:
		EffectProcessor() = default;
		~EffectProcessor();

		void Initialize(AsyncMessageBus* asyncMessageBus, const EffectProcessorConfig& config);
		void Shutdown();

		void SyncNodes();
		void SetOutputChannels(uint32_t outChannels) { m_outChannels = outChannels; }

		void ProcessEffectChain(EffectChain& chain, float* buffer, uint32_t frameCount);

		bool FlushEffect(EffectHandle handle);

	private:
		void SubscribeToCommands();
		void UnsubscribeFromCommands();

		void HandleAllocateEffectCommand(const AsyncMixerCommand& cmd);
		void HandleFreeEffectCommand(const AsyncMixerCommand& cmd);

		void ProcessSlot(EffectSlot& slot, float* buffer, uint32_t frameCount);
		bool IsAlive(EffectHandle handle);
		void ProcessEffect(EffectHandle handle, float* buffer, uint32_t frameCount);

		AsyncMessageBus* m_asyncMessageBus = nullptr;

		MixerPoolView<Biquad, BiquadParams> m_biquadPool;

		uint32_t m_maxSamplesPeriod = 0;
		uint32_t m_outChannels = 0;
		uint32_t m_outSampleRate = 0;
		float m_crossFadeDelta = 0.0f;

		std::unique_ptr<float[]> m_scratchBuffer;
	};
}
