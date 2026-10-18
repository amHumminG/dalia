#pragma once

#include "core/InternalState.h" // TODO: Implement internal state header
#include "dalia/EffectControl.h"
#include "mixer/effects/Biquad.h"

namespace dalia {

	struct EffectTraits;
	template<typename TParams>

	template<>
	struct EffectTraits<BiquadParams> {
		static constexpr EffectType TYPE = EffectType::Biquad;
		using DSPType = Biquad;
		using MirrorType = BiquadMirror;

		static auto& GetPool(EngineInternalState& state) { return state.biquads; }
	};
}
