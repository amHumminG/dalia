#pragma once

#include <variant>
#include <cstddef>

namespace dalia::utility {

    const char* GetStbVorbisErrorString(int error);

	// Universal helper that instantiates a variant based on a runtime index
	template <typename TVariant, size_t I = 0>
	constexpr void EmplaceVariantByIndex(TVariant& variant, size_t targetIndex) {
		if constexpr (I < std::variant_size_v<TVariant>) {
			if (I == targetIndex) {
				variant.template emplace<I>();
				return;
			}

			EmplaceVariantByIndex<TVariant, I + 1>(variant, targetIndex);
		}
	}

}
