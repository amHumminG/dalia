#pragma once

#include "dalia/SoundControl.h"
#include "core/Types.h"

namespace dalia {

	/// @brief Lightweight read-only view of a sound asset.
	struct SoundDescriptor {
		SoundType soundType = SoundType::None;
		LoadState loadState = LoadState::Unloaded;
		uint32_t channels = 0;
		uint32_t sampleRate = 0;
		uint32_t totalFrames = 0;
		double lengthInSeconds = 0;

		const float* pcmData = nullptr; // For resident
		const char* filepath = nullptr; // For streaming
	};
}
