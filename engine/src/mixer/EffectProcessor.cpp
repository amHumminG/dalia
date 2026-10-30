#include "mixer/EffectProcessor.h"
#include "mixer/dsp/Buffer.h"
#include "async/messaging/AsyncMessageBus.h"
#include "async/messaging/AsyncMixerMessaging.h"
#include "core/Constants.h"

#include <cstring>

namespace dalia {

	EffectProcessor::~EffectProcessor() {
		Shutdown();
	}

	void EffectProcessor::Initialize(AsyncMessageBus* asyncMessageBus, const EffectProcessorConfig& config) {
		m_asyncMessageBus  = asyncMessageBus;
		m_biquadPool       = config.biquadPool;
		m_maxSamplesPeriod = config.maxSamplesPeriod;
		m_outChannels      = config.outChannels;
		m_outSampleRate    = config.outSampleRate;

		constexpr float FADE_DURATION_SECONDS = 0.1f;
		m_crossFadeDelta = 1.0f / (static_cast<float>(m_outSampleRate) * FADE_DURATION_SECONDS);

		m_scratchBuffer = std::make_unique<float[]>(m_maxSamplesPeriod);

		SubscribeToCommands();
	}

	void EffectProcessor::Shutdown() {
		if (!m_asyncMessageBus) return;

		UnsubscribeFromCommands();
		m_asyncMessageBus = nullptr;
	}

	void EffectProcessor::SyncNodes() {
		m_biquadPool.SyncParams();
	}

	void EffectProcessor::ProcessEffectChain(EffectChain& chain, float* buffer, uint32_t frameCount) {
		for (EffectSlot& slot : chain.slots) {
			if (slot.state == EffectSlotState::None) continue;
			ProcessSlot(slot, buffer, frameCount);
		}
	}

	bool EffectProcessor::FlushEffect(EffectHandle handle) {
		uint32_t eIndex = handle.GetIndex();
		uint32_t eGen	= handle.GetGeneration();

		switch (handle.GetType()) {
			case EffectType::Biquad: {
				if (eIndex >= m_biquadPool.capacity) return false;

				Biquad& biquad = m_biquadPool.nodes[eIndex];
				if (biquad.gen != eGen) return false;

				biquad.Flush();
				return true;
			}
			default:
				return false;
		}
	}

	void EffectProcessor::SubscribeToCommands() {
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AllocateEffect,
			[this](const AsyncMixerCommand& cmd) { HandleAllocateEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FreeEffect,
			[this](const AsyncMixerCommand& cmd) { HandleFreeEffectCommand(cmd); });
	}

	void EffectProcessor::UnsubscribeFromCommands() {
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AllocateEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FreeEffect);
	}

	void EffectProcessor::HandleAllocateEffectCommand(const AsyncMixerCommand& cmd) {
		uint32_t eIndex = cmd.targetIndex;
		uint32_t eGen = cmd.targetGen;

		switch (cmd.data.effect.type) {
			case EffectType::Biquad: {
				Biquad& biquad = m_biquadPool.nodes[eIndex];
				biquad.gen = eGen;
				return;
			}
			default:
				return;
		}
	}

	void EffectProcessor::HandleFreeEffectCommand(const AsyncMixerCommand& cmd) {
		uint32_t eIndex = cmd.targetIndex;
		uint32_t eGen = cmd.targetGen;

		switch (cmd.data.effect.type) {
			case EffectType::Biquad: {
				Biquad& biquad = m_biquadPool.nodes[eIndex];
				if (biquad.gen == eGen) biquad.Reset();
				return;
			}
			default:
				return;
		}
	}

	void EffectProcessor::ProcessSlot(EffectSlot& slot, float* buffer, uint32_t frameCount) {
		if (!IsAlive(slot.handle)) {
			slot.Reset();
			return;
		}

		float* processBuffer = buffer;
		uint32_t sampleCount = frameCount * m_outChannels;

		if (slot.state != EffectSlotState::Active) {
			std::memcpy(m_scratchBuffer.get(), buffer, sampleCount * sizeof(float));
			processBuffer = m_scratchBuffer.get();
		}

		ProcessEffect(slot.handle, processBuffer, frameCount);

		if (slot.state != EffectSlotState::Active) {
			float targetMix = (slot.state == EffectSlotState::FadingIn) ? GAIN_DEFAULT : GAIN_SILENCE;

			dsp::ApplyBlockCrossFade(
				buffer,
				processBuffer,
				frameCount,
				m_outChannels,
				slot.currentMix,
				targetMix,
				m_crossFadeDelta
			);

			if (slot.state == EffectSlotState::FadingIn && slot.currentMix >= GAIN_DEFAULT) {
				slot.state = EffectSlotState::Active;
			}
			else if (slot.state == EffectSlotState::FadingOut && slot.currentMix <= GAIN_SILENCE) {
				slot.Reset();
			}
		}
	}

	bool EffectProcessor::IsAlive(EffectHandle handle) {
		uint32_t eIndex = handle.GetIndex();
		uint32_t eGen	= handle.GetGeneration();

		switch (handle.GetType()) {
			case EffectType::Biquad:
				return eIndex < m_biquadPool.capacity && m_biquadPool.nodes[eIndex].gen == eGen;
			default:
				return false;
		}
	}

	void EffectProcessor::ProcessEffect(EffectHandle handle, float* buffer, uint32_t frameCount) {
		uint32_t eIndex = handle.GetIndex();

		switch (handle.GetType()) {
			case EffectType::Biquad:
				m_biquadPool.nodes[eIndex].Process(buffer, frameCount, m_outChannels, m_outSampleRate);
				break;
			default:
				break;
		}
	}
}
