#pragma once

#include "dalia/EffectControl.h"
#include "core/Constants.h"

#include <array>
#include <cstdint>

namespace dalia {

	enum class EffectSlotState : uint8_t {
		None,
		FadingIn,
		Active,
		FadingOut
	};

	struct EffectSlot {
		EffectHandle handle = INVALID_EFFECT_HANDLE;
		EffectSlotState state = EffectSlotState::None;
		float currentMix = GAIN_SILENCE;

		void Reset() {
			handle = INVALID_EFFECT_HANDLE;
			state = EffectSlotState::None;
			currentMix = GAIN_SILENCE;
		}
	};

	struct EffectChain {
		std::array<EffectSlot, MAX_EFFECTS_PER_BUS> slots{};

		void Reset() {
			for (EffectSlot& slot : slots) slot.Reset();
		}

		void Attach(uint32_t slotIndex, EffectHandle handle) {
			if (slotIndex >= MAX_EFFECTS_PER_BUS) return;
			EffectSlot& slot = slots[slotIndex];
			slot.handle = handle;
			slot.state = EffectSlotState::FadingIn;
			slot.currentMix = GAIN_SILENCE;
		}

		void FadeOut(uint32_t slotIndex, EffectHandle handle) {
			if (slotIndex >= MAX_EFFECTS_PER_BUS) return;
			EffectSlot& slot = slots[slotIndex];
			if (slot.handle == handle && slot.state != EffectSlotState::None) {
				slot.state = EffectSlotState::FadingOut;
			}
		}

		void ForceDetach(uint32_t slotIndex, EffectHandle handle) {
			if (slotIndex >= MAX_EFFECTS_PER_BUS) return;
			EffectSlot& slot = slots[slotIndex];
			if (slot.handle == handle) slot.Reset();
		}

		void PurgeEffect(EffectHandle handle) {
			for (EffectSlot& slot : slots) {
				if (slot.handle == handle) slot.Reset();
			}
		}
	};
}
