#pragma once

#include "core/Constants.h"

#include <cstdint>

namespace dalia {

	struct ResamplerState {
		float fractionalPhase = 0.0f;
		float history[CHANNELS_MAX] = {0.0f};
	};

	void ProcessResampler(
		ResamplerState& state,
		const float* sourceData,
		uint32_t maxSourceFrames,
		uint32_t channels,
		float* outBuffer,
		uint32_t maxOutputFrames,
		float phaseIncrement,
		uint32_t& outFramesGenerated,
		uint32_t& sourceFramesConsumed
	);
}