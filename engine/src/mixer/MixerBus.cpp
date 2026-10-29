#include "mixer/Mixer.h"
#include "mixer/nodes/Bus.h"
#include "core/Logger.h"
#include "mixer/dsp/Buffer.h"
#include "mixer/nodes/effects/Biquad.h"
#include "async/messaging/AsyncMixerMessaging.h"

namespace dalia {

	void Mixer::HandleAllocateBusCommand(const AsyncMixerCommand& cmd) {
		Bus& bus = m_busPool.nodes[cmd.targetIndex];

		bus.currentParentIndex = cmd.data.setParent.parentIndex;
		bus.targetParentIndex = cmd.data.setParent.parentIndex;
		bus.isActive = true;

		m_isMixOrderDirty = true;
	}

	void Mixer::HandleFreeBusCommand(const AsyncMixerCommand& cmd) {
		uint32_t bIndex = cmd.targetIndex;

		m_busPool.nodes[bIndex].Reset();
		m_isMixOrderDirty = true;
	}

	void Mixer::HandleSetBusParentCommand(const AsyncMixerCommand& cmd) {
		uint32_t bIndex = cmd.targetIndex;
		uint32_t bIndexParent = cmd.data.setParent.parentIndex;

		m_busPool.nodes[bIndex].targetParentIndex = bIndexParent;
	}

	bool Mixer::ResolveBusStates() {
		bool topologyChanged = false;

		for (uint32_t bIndex = 0; bIndex < m_busPool.capacity; bIndex++) {
			Bus& bus = m_busPool.nodes[bIndex];
			if (!bus.isActive) continue;

			bool requiresSilence = false;

			// Do we need to duck?
			if (bus.currentParentIndex != bus.targetParentIndex) requiresSilence = true;

			if (requiresSilence) bus.targetFadeGain = GAIN_SILENCE;

			if (math::NearlyEqual(bus.currentFadeGain, 0.0f, EPSILON_GAIN)) {
				// Bus is silent

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

	uint32_t Mixer::RenderBuses(uint32_t frameCount) {
		std::span activeMixOrder = std::span(m_mixOrder.get(), m_mixOrderSize);
		uint32_t busesRendered = 0;

		for (uint32_t bIndex : activeMixOrder) {
			busesRendered++;

			Bus& bus = m_busPool.nodes[bIndex];
			float* buffer = GetBusBuffer(bIndex);

			for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
				EffectSlot& slot = bus.effectSlots[i];
				if (slot.state == EffectSlotState::None) continue;
				ApplyBusEffect(buffer, slot, frameCount);
			}

			dsp::ApplyGainAndFade(
				buffer, frameCount, m_outChannels,
				bus.currentGain, bus.params.gain, m_smoothingCoefficient,
				bus.currentFadeGain, bus.targetFadeGain, m_fadeStep
			);

			if (bus.currentParentIndex != NO_PARENT) {
				float* parentBuffer = GetBusBuffer(bus.currentParentIndex);
				dsp::MixToBuffer(parentBuffer, buffer, frameCount * m_outChannels);
			}
		}

		return busesRendered;
	}

	void Mixer::ApplyBusEffect(float* buffer, EffectSlot& slot, uint32_t frameCount) {
		uint32_t eIndex = slot.handle.GetIndex();
		uint32_t eGen = slot.handle.GetGeneration();
		EffectType eType = slot.handle.GetType();

		// Validate that the effect instance is still alive before mixing it
		switch (eType) {
			case EffectType::Biquad: {
				if (m_biquadPool.nodes[eIndex].gen != eGen) {
					slot.Reset();
					return;
				}
				break;
			}
			default:
				slot.Reset();
				break;
		}

		float* processBuffer = buffer;
		uint32_t sampleCount = frameCount * m_outChannels;

		if (slot.state != EffectSlotState::Active) {
			std::memcpy(m_mixScratchBuffer.get(), buffer, sampleCount * sizeof(float));
			processBuffer = m_mixScratchBuffer.get();
		}

		switch (eType) {
			case EffectType::Biquad: {
				Biquad& biquad = m_biquadPool.nodes[eIndex];
				biquad.Process(processBuffer, frameCount, m_outChannels, m_outSampleRate);

				break;
			}
			default:
				break;
		}

		// Process DSP
		if (slot.state != EffectSlotState::Active) {
			float fadeDurationInSeconds = 0.01f;
			float fadeDelta = 1.0f / (static_cast<float>(m_outSampleRate) * fadeDurationInSeconds);

			float targetMix = 0.0f;
			if (slot.state == EffectSlotState::FadingIn) targetMix = 1.0f;
			else if (slot.state == EffectSlotState::FadingOut) targetMix = 0.0f;

			dsp::ApplyBlockCrossFade(
				buffer,
				processBuffer,
				frameCount,
				m_outChannels,
				slot.currentMix,
				targetMix,
				fadeDelta
			);

			if (slot.state == EffectSlotState::FadingIn && slot.currentMix >= GAIN_DEFAULT) {
				slot.state = EffectSlotState::Active;
			}
			else if (slot.state == EffectSlotState::FadingOut && slot.currentMix <= GAIN_SILENCE) {
				slot.Reset();
			}
		}
	}
}
