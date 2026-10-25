#pragma once

#include "dalia/SoundControl.h"
#include "core/Types.h"

namespace dalia {

	/// @brief Lightweight read-only view of a sound asset passed into MixController.
	struct SoundDescriptor {
		SoundHandle handle = INVALID_SOUND_HANDLE;
		LoadState state = LoadState::Unloaded;
		uint32_t channels = 0;
		uint32_t sampleRate = 0;
		uint32_t frameCount = 0;
		// double lengthInSeconds = 0;
		const float* pcmData = nullptr;
		const char* filepath = nullptr;
	};
}
