#pragma once

#include "core/Constants.h"

#include <cstdint>

namespace dalia {

	struct EffectMirror {
		uint32_t gen = START_GENERATION;
		uint32_t busIndex = NO_INDEX;
		uint32_t effectSlot = NO_INDEX;
		EffectState state = EffectState::None; // I don't think we want this? Or do we? I'm pretty sure it's mixer-only

		bool IsAttached() const { return busIndex != NO_INDEX; }

		void ResetBase() {
			gen++;
			if (gen == NO_GENERATION) gen = START_GENERATION;
			busIndex = NO_INDEX;
			effectSlot = NO_INDEX;
			state = EffectState::None;
		}
	};
}