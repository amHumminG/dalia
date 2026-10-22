#include "io/codec/VorbisCodec.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <string_view>

namespace dalia {

	const VorbisCodec& VorbisCodec::GetInstance() {
		static VorbisCodec instance;
		return instance;
	}

	Result VorbisCodec::ProbeFile(const char* filepath, AudioFormatInfo& info) const {
		int error;
		stb_vorbis* decoder = stb_vorbis_open_filename(filepath, &error, nullptr);
		if (!decoder) return Result::FileReadError;

		stb_vorbis_info decoderInfo = stb_vorbis_get_info(decoder);
		info.channels = decoderInfo.channels;
		info.sampleRate = decoderInfo.sample_rate;
		info.totalFrames = stb_vorbis_stream_length_in_samples(decoder);
		info.lengthInSeconds = stb_vorbis_stream_length_in_seconds(decoder);

		stb_vorbis_close(decoder);
		return Result::Ok;
	}

	Result VorbisCodec::DecodeFile(const char* filepath, std::vector<float>& pcm, AudioFormatInfo& info) const {
		int error;
		stb_vorbis* decoder = stb_vorbis_open_filename(filepath, &error, nullptr);
		if (!decoder) return Result::FileReadError;

		stb_vorbis_info decoderInfo = stb_vorbis_get_info(decoder);
		info.channels = decoderInfo.channels;
		info.sampleRate = decoderInfo.sample_rate;
		info.totalFrames = stb_vorbis_stream_length_in_samples(decoder);
		// info.lengthInSeconds = stb_vorbis_stream_length_in_seconds(decoder);
		info.lengthInSeconds = static_cast<double>(info.totalFrames) / decoderInfo.sample_rate;

		uint32_t totalSamples = info.totalFrames * info.channels;
		pcm.resize(totalSamples);

		int framesDecoded = stb_vorbis_get_samples_float_interleaved(
			decoder, decoderInfo.channels, pcm.data(), static_cast<int>(totalSamples)
		);

		if (framesDecoded <= 0) {
			stb_vorbis_close(decoder);
			return Result::FileReadError;
		}

		if (framesDecoded != static_cast<int>(info.totalFrames)) {
			info.totalFrames = static_cast<uint32_t>(framesDecoded);
			info.lengthInSeconds = static_cast<double>(framesDecoded) / decoderInfo.sample_rate;
			pcm.resize(static_cast<size_t>(framesDecoded) * info.channels);
		}

		stb_vorbis_close(decoder);
		return Result::Ok;
	}

	Result VorbisCodec::OpenStream(const char* filepath, void*& decoderHandle, AudioFormatInfo& info) const {
		int error;
		stb_vorbis* decoder = stb_vorbis_open_filename(filepath, &error, nullptr);
		if (!decoder) return Result::FileReadError;

		stb_vorbis_info decoderInfo = stb_vorbis_get_info(decoder);
		info.channels = decoderInfo.channels;
		info.sampleRate = decoderInfo.sample_rate;
		info.totalFrames = stb_vorbis_stream_length_in_samples(decoder);

		decoderHandle = static_cast<void*>(decoder);
		return Result::Ok;
	}

	uint32_t VorbisCodec::DecodeStreamFrames(void* decoderHandle, uint32_t channels, float* buffer,
		uint32_t framesToRead) const {
		auto decoder = static_cast<stb_vorbis*>(decoderHandle);
		int floatsToRead = static_cast<int>(framesToRead * channels);
		int framesRead = stb_vorbis_get_samples_float_interleaved(
			decoder, static_cast<int>(channels), buffer, floatsToRead
		);
		return static_cast<uint32_t>(std::max(0, framesRead));
	}

	Result VorbisCodec::SeekStreamFrame(void* decoderHandle, uint32_t targetFrame) const {
		stb_vorbis* decoder = static_cast<stb_vorbis*>(decoderHandle);
		stb_vorbis_seek_frame(decoder, targetFrame);
		return Result::Ok;
	}

	void VorbisCodec::CloseStream(void*& decoderHandle) const {
		if (decoderHandle) {
			stb_vorbis_close(static_cast<stb_vorbis*>(decoderHandle));
			decoderHandle = nullptr;
		}
	}
}
