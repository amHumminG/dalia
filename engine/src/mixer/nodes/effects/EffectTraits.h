#pragma once
#include "dalia/EffectControl.h"
#include "mixer/nodes/effects/Biquad.h"

namespace dalia {
	class MixController;

	template<typename TParams>
	struct EffectTraits;

	template<>
	struct EffectTraits<BiquadParams> {
		static constexpr EffectType TYPE = EffectType::Biquad;
		using DSPType = Biquad;
		using MirrorType = BiquadMirror;

		static auto& GetPool(MixController& controller);
	};
}
