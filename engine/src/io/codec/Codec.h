#pragma once

#include "dalia/Result.h"

#include <cstdint>
#include <vector>

namespace dalia {

	struct AudioFormatInfo {
		uint32_t channels = 0;
		uint32_t sampleRate = 0;
		uint32_t totalFrames = 0;
		double lengthInSeconds = 0.0f;
	};

	class Codec {
	public:
		virtual ~Codec() = default;

		// --- Non-Stateful ---
		virtual Result ProbeFile(const char* filepath, AudioFormatInfo& info) const = 0;
		virtual Result DecodeFile(const char* filepath, std::vector<float>& pcm, AudioFormatInfo& info) const = 0;

		// --- Stateful ---
		virtual Result OpenStream(const char* filepath, void*& decoderHandle, AudioFormatInfo& info) const = 0;
		virtual uint32_t DecodeStreamFrames(void* decoderHandle, uint32_t channels, float* buffer, uint32_t framesToRead) const = 0;
		virtual Result SeekStreamFrame(void* decoderHandle, uint32_t targetFrame) const = 0;
		virtual void CloseStream(void*& decoderHandle) const = 0;

		static const Codec* GetCodecForFile(const char* filepath);
	};
}