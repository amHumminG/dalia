#pragma once

#include "mixer/effects/BiquadFilter.h"

#include <variant>

namespace dalia {

	// --- Compile time config-to-DSP lookup table ---
	template <typename TConfig>
	struct DSPMapper;

	template<>
	struct DSPMapper<BiquadConfig> {
		using Type = BiquadFilter;
	};

	// ---

	using EffectParams = std::variant<
		std::monostate,
		BiquadConfig
	>;

	using DSP = std::variant<
		std::monostate,
		BiquadFilter
	>;

	struct Effect {
		uint32_t gen = NO_GENERATION;
		DSP dsp;

		void Reset() {
			dsp.emplace<std::monostate>();
		}
	};

	struct EffectMirror {
		uint32_t gen = START_GENERATION;
		EffectParams params;
		bool isParamsDirty = false;

		void Reset() {
			gen++;
			params = EffectParams{};
			isParamsDirty = false;
		}
	};

}