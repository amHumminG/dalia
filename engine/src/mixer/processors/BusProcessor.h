#pragma once

#include "dalia/Result.h"
#include "core/StateSyncPool.h"
#include "mixer/MixGraphCompiler.h"
#include "mixer/nodes/Bus.h"

#include <cstdint>
#include <memory>
#include <span>

namespace dalia {

	class AsyncMessageBus;
	class EffectProcessor;
	struct AsyncMixerCommand;

	struct BusProcessorConfig {
		MixerPoolView<Bus, BusParams> busPool;
		uint32_t maxSamplesPeriod = 0;
		uint32_t outChannels = CHANNELS_MONO;
		uint32_t outSampleRate = 48000;
		float smoothingCoefficient = 0.0f;
		float fadeStep = 0.0f;
	};

	class BusProcessor {
	public:
		BusProcessor() = default;
		~BusProcessor();

		void Initialize(
			AsyncMessageBus* asyncMessageBus,
			EffectProcessor* effectProcessor,
			const BusProcessorConfig& config
		);
		void Shutdown();

		void SyncNodes() { m_busPool.SyncParams(); }
		void SetOutputChannels(uint32_t outChannels) { m_outChannels = outChannels; }

		void ClearBuffers();

		bool ResolveStatesAndCompileGraph();

		uint32_t RenderBuses(uint32_t frameCount);

		[[nodiscard]] float* GetBusBuffer(uint32_t busIndex) const;

	private:
		void SubscribeToCommands();
		void UnsubscribeFromCommands();

		// --- Command Handling ---
		void HandleAllocateBusCommand(const AsyncMixerCommand& cmd);
		void HandleFreeBusCommand(const AsyncMixerCommand& cmd);
		void HandleSetBusParentCommand(const AsyncMixerCommand& cmd);

		void HandleAttachEffectCommand(const AsyncMixerCommand& cmd);
		void HandleFadeDetachEffectCommand(const AsyncMixerCommand& cmd);
		void HandleForceDetachEffectCommand(const AsyncMixerCommand& cmd);

		// --- State Resolution Helpers ---
		bool ResolveBusStates();
		bool CompileMixGraph();

		AsyncMessageBus* m_asyncMessageBus = nullptr;
		EffectProcessor* m_effectProcessor = nullptr;

		MixerPoolView<Bus, BusParams> m_busPool;

		uint32_t m_maxSamplesPeriod = 0;
		uint32_t m_outChannels = 0;
		uint32_t m_outSampleRate = 0;
		float m_smoothingCoefficient = 0.0;
		float m_fadeStep = 0.0f;

		std::unique_ptr<MixGraphCompiler>	m_mixGraphCompiler;
		std::unique_ptr<uint32_t[]>			m_mixOrder;
		uint32_t							m_mixOrderSize = 0;
		bool								m_isMixOrderDirty = true;

		std::unique_ptr<float[]>			m_busBufferPool;
	};
}