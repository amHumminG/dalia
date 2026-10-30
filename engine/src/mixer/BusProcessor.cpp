#include "mixer/BusProcessor.h"
#include "mixer/dsp/Gain.h"
#include "mixer/dsp/Buffer.h"
#include "async/messaging/AsyncMessageBus.h"
#include "async/messaging/AsyncMixerMessaging.h"
#include "core/Logger.h"
#include "core/Math.h"

#include <cstring>

#include "EffectProcessor.h"

namespace dalia {

	BusProcessor::~BusProcessor() {
		Shutdown();
	}

	void BusProcessor::Initialize(AsyncMessageBus* asyncMessageBus, EffectProcessor* effectProcessor,
		const BusProcessorConfig& config) {

		m_asyncMessageBus		= asyncMessageBus;
		m_effectProcessor		= effectProcessor;
		m_busPool				= config.busPool;
		m_maxSamplesPeriod		= config.maxSamplesPeriod;
		m_outChannels			= config.outChannels;
		m_outSampleRate			= config.outSampleRate;
		m_smoothingCoefficient	= config.smoothingCoefficient;
		m_fadeStep				= config.fadeStep;

		const auto busCount = static_cast<uint32_t>(m_busPool.capacity);
		m_mixGraphCompiler	= std::make_unique<MixGraphCompiler>(busCount);
		m_mixOrder			= std::make_unique<uint32_t[]>(busCount);
		m_busBufferPool		= std::make_unique<float[]>(busCount * m_maxSamplesPeriod);
		m_isMixOrderDirty	= true;

		SubscribeToCommands();
	}

	void BusProcessor::Shutdown() {
		if (!m_asyncMessageBus) return;

		UnsubscribeFromCommands();
		m_asyncMessageBus = nullptr;
		m_effectProcessor = nullptr;
	}

	void BusProcessor::ClearBuffers() {
		std::memset(m_busBufferPool.get(), 0, m_busPool.capacity * m_maxSamplesPeriod * sizeof(float));
	}

	bool BusProcessor::ResolveStatesAndCompileGraph() {
		m_isMixOrderDirty |= ResolveBusStates();

		if (m_isMixOrderDirty) {
			if (!CompileMixGraph()) return false;
			m_isMixOrderDirty = false;
		}
		return true;
	}

	uint32_t BusProcessor::RenderBuses(uint32_t frameCount) {
		std::span<const uint32_t> activeMixOrder(m_mixOrder.get(), m_mixOrderSize);
		uint32_t busesRendered = 0;

		for (uint32_t bIndex : activeMixOrder) {
			busesRendered++;

			Bus& bus = m_busPool.nodes[bIndex];
			float* buffer = GetBusBuffer(bIndex);

			// Process Effect chain
			m_effectProcessor->ProcessEffectChain(
				bus.effectChain,
				buffer,
				frameCount
			);

			// Apply gain and fade
			dsp::ApplyGainAndFade(
				buffer,
				frameCount,
				m_outChannels,
				bus.currentGain,
				bus.params.gain,
				m_smoothingCoefficient,
				bus.currentFadeGain,
				bus.targetFadeGain,
				m_fadeStep
			);

			// Mix into parent
			if (bus.currentParentIndex != NO_PARENT) {
				float* parentBuffer = GetBusBuffer(bus.currentParentIndex);
				dsp::MixToBuffer(parentBuffer, buffer, frameCount * m_outChannels);
			}
		}

		return busesRendered;
	}

	float* BusProcessor::GetBusBuffer(uint32_t busIndex) const {
		if (busIndex >= m_busPool.capacity) return nullptr;
		return m_busBufferPool.get() + (static_cast<size_t>(busIndex) * m_maxSamplesPeriod);
	}

	void BusProcessor::SubscribeToCommands() {
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AllocateBus,
			[this](const AsyncMixerCommand& cmd) { HandleAllocateBusCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FreeBus,
			[this](const AsyncMixerCommand& cmd) { HandleFreeBusCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::SetBusParent,
			[this](const AsyncMixerCommand& cmd) { HandleSetBusParentCommand(cmd); });

		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AttachEffect,
			[this](const AsyncMixerCommand& cmd) { HandleAttachEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FadeDetachEffect,
			[this](const AsyncMixerCommand& cmd) { HandleFadeDetachEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::ForceDetachEffect,
			[this](const AsyncMixerCommand& cmd) { HandleForceDetachEffectCommand(cmd); });
	}

	void BusProcessor::UnsubscribeFromCommands() {
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AllocateBus);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FreeBus);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SetBusParent);

		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AttachEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FadeDetachEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::ForceDetachEffect);
	}

	void BusProcessor::HandleAllocateBusCommand(const AsyncMixerCommand& cmd) {
		Bus& bus = m_busPool.nodes[cmd.targetIndex];

		bus.currentParentIndex = cmd.data.setParent.parentIndex;
		bus.targetParentIndex = cmd.data.setParent.parentIndex;
		bus.isActive = true;
		m_isMixOrderDirty = true;
	}

	void BusProcessor::HandleFreeBusCommand(const AsyncMixerCommand& cmd) {
		m_busPool.nodes[cmd.targetIndex].Reset();
		m_isMixOrderDirty = true;
	}

	void BusProcessor::HandleSetBusParentCommand(const AsyncMixerCommand& cmd) {
		m_busPool.nodes[cmd.targetIndex].targetParentIndex = cmd.data.setParent.parentIndex;
	}

	void BusProcessor::HandleAttachEffectCommand(const AsyncMixerCommand& cmd) {
		auto handle = EffectHandle::Create(cmd.targetIndex, cmd.targetGen, cmd.data.effect.type);
		if (!m_effectProcessor->FlushEffect(handle)) return;

		// Purge effect if fading out
		for (Bus& bus : m_busPool.nodes) bus.effectChain.PurgeEffect(handle);

		m_busPool.nodes[cmd.data.effect.busIndex].effectChain.Attach(cmd.data.effect.effectSlot, handle);
	}

	void BusProcessor::HandleFadeDetachEffectCommand(const AsyncMixerCommand& cmd) {
		auto handle = EffectHandle::Create(cmd.targetIndex, cmd.targetGen, cmd.data.effect.type);
		m_busPool.nodes[cmd.data.effect.busIndex].effectChain.FadeOut(cmd.data.effect.effectSlot, handle);
	}

	void BusProcessor::HandleForceDetachEffectCommand(const AsyncMixerCommand& cmd) {
		auto handle = EffectHandle::Create(cmd.targetIndex, cmd.targetGen, cmd.data.effect.type);
		m_busPool.nodes[cmd.data.effect.busIndex].effectChain.ForceDetach(cmd.data.effect.effectSlot, handle);
	}

	bool BusProcessor::ResolveBusStates() {
		bool topologyChanged = false;

		for (uint32_t bIndex = 0; bIndex < m_busPool.capacity; bIndex++) {
			Bus& bus = m_busPool.nodes[bIndex];
			if (!bus.isActive) continue;

			// Do we need to duck?
			bool requiresSilence = (bus.currentParentIndex != bus.targetParentIndex);
			if (requiresSilence) bus.targetFadeGain = GAIN_SILENCE;

			// Check if bus is silent
			if (math::NearlyEqual(bus.currentFadeGain, 0.0f, EPSILON_GAIN)) {
				if (bus.currentParentIndex != bus.targetParentIndex) {
					bus.currentParentIndex = bus.targetParentIndex;
					DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Re-routed bus to target");
					topologyChanged = true;
				}
			}

			if (!requiresSilence) bus.targetFadeGain = GAIN_DEFAULT;
		}

		return topologyChanged;
	}

	bool BusProcessor::CompileMixGraph() {
		m_mixOrderSize = m_mixGraphCompiler->Compile(
			m_busPool.nodes,
			std::span(m_mixOrder.get(), m_busPool.capacity)
		);

		if (m_mixOrderSize == 0) {
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Failed to compile mix graph. Cycle detected.");
			return false;
		}

		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Recompiled bus graph.");
		return true;
	}
}
