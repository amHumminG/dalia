#pragma once

#include "engine/EngineInternalState.h"
#include "dalia/EffectControl.h"
#include "engine/EngineInternalState.h"
#include "mixer/effects/Biquad.h"

namespace dalia {
	class MixController;

	template<typename TParams>
	struct EffectTraits;

	template<>
	struct EffectTraits<BiquadParams> {
		static constexpr EffectType TYPE = EffectType::Biquad;
		using DSPType = Biquad;
		using MirrorType = BiquadMirror;

		static auto& GetPool(MixController& controller); // TODO: Fix
	};
}
