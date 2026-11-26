#pragma once
#include "dalia/Effect.hpp"
#include "mixer/nodes/effects/Biquad.hpp"

namespace dalia {
	class MixController;

	template <typename TParams>
	struct EffectTraits;

	template <>
	struct EffectTraits<BiquadParams> {
		static constexpr EffectType TYPE = EffectType::Biquad;
		using DSPType                    = Biquad;
		using MirrorType                 = BiquadMirror;

		static auto& GetPool(MixController& controller);
	};

	template<>
	struct EffectTraits<FdnReverbParams>
	{
		static constexpr EffectType TYPE = EffectType::FdnReverb;
		using DSPType					 = FdnReverb;
		using MirrorType				 = FdnReverbMirror;

		static auto& GetPool(MixController& controller);
	};
}
