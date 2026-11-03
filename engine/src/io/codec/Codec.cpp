#include "io/codec/Codec.h"

#include "io/codec/VorbisCodec.h"

#include <string_view>

namespace dalia {

	const Codec* Codec::GetCodecForFile(const char* filepath) {
		std::string_view path(filepath);
		if (path.ends_with(".ogg")) return &VorbisCodec::GetInstance();
		// Add more decoders here

		return nullptr;
	}
}
