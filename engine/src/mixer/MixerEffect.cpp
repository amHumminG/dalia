#include "mixer/Mixer.h"
#include "async/messaging/AsyncMixerMessaging.h"
#include "mixer/nodes/effects/Biquad.h"
#include "core/Constants.h"
#include "mixer/nodes/Bus.h"

namespace dalia {

	void Mixer::HandleAllocateEffectCommand(const AsyncMixerCommand& cmd) {
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

	void Mixer::HandleFreeEffectCommand(const AsyncMixerCommand& cmd) {
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

	void Mixer::HandleAttachEffectCommand(const AsyncMixerCommand& cmd) {
		uint32_t eIndex = cmd.targetIndex;
		uint32_t eGen = cmd.targetGen;
		EffectType eType = cmd.data.effect.type;

		switch (eType) {
			case EffectType::Biquad: {
				Biquad& biquad = m_biquadPool.nodes[eIndex];
				if (biquad.gen != eGen) return;

				biquad.Flush();
				break;
			}
			default:
				return;
		}

		// Ensure the effect is not still fading out on another bus slot
		auto eHandle = EffectHandle::Create(eIndex, eGen, eType);
		for (Bus& bus : m_busPool.nodes) {
			for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
				if (bus.effectSlots[i].handle == eHandle) {
					bus.effectSlots[i].Reset();
				}
			}
		}

		uint32_t bIndex = cmd.data.effect.busIndex;
		uint32_t effectSlot = cmd.data.effect.effectSlot;

		EffectSlot& slot = m_busPool.nodes[bIndex].effectSlots[effectSlot];
		slot.handle = eHandle;
		slot.state = EffectSlotState::FadingIn;
		slot.currentMix = GAIN_SILENCE;
	}

	void Mixer::HandleFadeDetachEffectCommand(const AsyncMixerCommand& cmd) {
		uint32_t eIndex = cmd.targetIndex;
		uint32_t eGen = cmd.targetGen;
		EffectType eType = cmd.data.effect.type;

		EffectSlot& slot = m_busPool.nodes[cmd.data.effect.busIndex].effectSlots[cmd.data.effect.effectSlot];
		if (slot.handle == EffectHandle::Create(eIndex, eGen, eType) && slot.state != EffectSlotState::None) {
			slot.state = EffectSlotState::FadingOut;
		}
	}

	void Mixer::HandleForceDetachEffectCommand(const AsyncMixerCommand& cmd) {
		uint32_t eIndex = cmd.targetIndex;
		uint32_t eGen = cmd.targetGen;
		EffectType eType = cmd.data.effect.type;

		EffectSlot& slot = m_busPool.nodes[cmd.data.effect.busIndex].effectSlots[cmd.data.effect.effectSlot];
		if (slot.handle == EffectHandle::Create(eIndex, eGen, eType)) slot.Reset();
	}
}
