#include "io/codec/VorbisCodec.h"
#include "core/Logger.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <string_view>

namespace dalia {

	namespace {
		// Helper function to translate stb_vorbis error codes into readable strings
	    const char* GetStbVorbisErrorString(int error) {
	        switch (error) {
	            case VORBIS__no_error:
	                return "No Error";
	            case VORBIS_need_more_data:
	                return "Need More Data";
	            case VORBIS_invalid_api_mixing:
	                return "Invalid API Mixing";
	            case VORBIS_outofmem:
	                return "Out of Memory";
	            case VORBIS_feature_not_supported:
	                return "Feature Not Supported (Uses Floor 0)";
	            case VORBIS_too_many_channels:
	                return "Too Many Channels";
	            case VORBIS_file_open_failure:
	                return "File Open Failure";
	            case VORBIS_seek_without_length:
	                return "Seek Without Length";
	            case VORBIS_unexpected_eof:
	                return "Unexpected EOF (Truncated File?)";
	            case VORBIS_seek_invalid:
	                return "Seek Invalid (Past EOF)";
	            case VORBIS_invalid_setup:
	                return "Invalid Setup (Corrupt Vorbis Stream)";
	            case VORBIS_invalid_stream:
	                return "Invalid Stream (Corrupt Vorbis Stream)";
	            case VORBIS_missing_capture_pattern:
	                return "Missing Capture Pattern (Ogg Error)";
	            case VORBIS_invalid_stream_structure_version:
	                return "Invalid Stream Structure Version (Ogg Error)";
	            case VORBIS_continued_packet_flag_invalid:
	                return "Continued Packet Flag Invalid (Ogg Error)";
	            case VORBIS_incorrect_stream_serial_number:
	                return "Incorrect Stream Serial Number (Ogg Error)";
	            case VORBIS_invalid_first_page:
	                return "Invalid First Page (Ogg Error)";
	            case VORBIS_bad_packet_type:
	                return "Bad Packet Type (Ogg Error)";
	            case VORBIS_cant_find_last_page:
	                return "Can't Find Last Page (Ogg Error)";
	            case VORBIS_seek_failed:
	                return "Seek Failed (Ogg Error)";
	            case VORBIS_ogg_skeleton_not_supported:
	                return "Ogg Skeleton Not Supported";
	            default:
	                return "Unknown STB Vorbis Error Code";
	        }
	    }
	}

	const VorbisCodec& VorbisCodec::GetInstance() {
		static VorbisCodec instance;
		return instance;
	}

	Result VorbisCodec::ProbeFile(const char* filepath, AudioFormatInfo& info) const {
		int error;
		stb_vorbis* decoder = stb_vorbis_open_filename(filepath, &error, nullptr);
		if (!decoder) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to open Vorbis for file: %s (Error: %s)", filepath,
				GetStbVorbisErrorString(error));
			return Result::FileReadError;
		}

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
		if (!decoder) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to open Vorbis for file: %s (Error: %s)", filepath,
				GetStbVorbisErrorString(error));
			return Result::FileReadError;
		}

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
		if (!decoder) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to open Vorbis stream for file: %s (Error: %s)", filepath,
				GetStbVorbisErrorString(error));
			return Result::FileReadError;
		}

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
		stb_vorbis_seek(decoder, targetFrame);
		return Result::Ok;
	}

	void VorbisCodec::CloseStream(void*& decoderHandle) const {
		if (decoderHandle) {
			stb_vorbis_close(static_cast<stb_vorbis*>(decoderHandle));
			decoderHandle = nullptr;
		}
	}
}
