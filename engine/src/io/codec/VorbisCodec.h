#pragma once
#include "io/codec/Codec.h"

namespace dalia {

	class VorbisCodec : public Codec {
	public:
		static const VorbisCodec& GetInstance();

		Result ProbeFile(const char* filepath, AudioFormatInfo& info) const override;
		Result DecodeFile(const char* filepath, std::vector<float>& pcm, AudioFormatInfo& info) const override;

		Result OpenStream(const char* filepath, void*& decoderHandle, AudioFormatInfo& info) const override;
		uint32_t DecodeStreamFrames(void* decoderHandle, uint32_t channels, float* buffer, uint32_t framesToRead) const override;
		Result SeekStreamFrame(void* decoderHandle, uint32_t targetFrame) const override;
		void CloseStream(void*& decoderHandle) const override;
	};
}
