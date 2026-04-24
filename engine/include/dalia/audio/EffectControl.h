#pragma once
#include <cstdint>

namespace dalia {

	enum class EffectType : uint8_t {
        None = 0,
        Biquad,
    };

	/// @brief Handle used to manage effect instances. This handle expires once the effect it is referencing has been
	/// destroyed.
	struct EffectHandle {
    public:
		/// @return true if the handle has referenced an effect at some point. Otherwise, false.
		bool IsValid() const { return rawId != 0; }

        bool operator==(const EffectHandle& other) const { return rawId == other.rawId; }
        bool operator!=(const EffectHandle& other) const { return rawId != other.rawId; }

        /// @return The type of effect the handle is referencing.
        EffectType GetType() const { return static_cast<EffectType>(rawId >> 56); }

		/// @return The underlying raw id of the handle.
        uint64_t GetRawId() const { return rawId; }

    private:
        friend class Engine;
        friend class RtSystem;
        friend struct EngineInternalState;

        static EffectHandle Create(uint32_t index, uint32_t generation, EffectType type) {
            EffectHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.rawId = typeBits | generationBits | indexBits;
            return handle;
        }

        static EffectHandle FromRawId(uint64_t rawId) {
            EffectHandle handle;
            handle.rawId = rawId;
            return handle;
        }

		uint32_t GetIndex() const { return static_cast<uint32_t>(rawId & 0xFFFFFFFF); }
		uint32_t GetGeneration() const { return static_cast<uint32_t>((rawId >> 32) & 0xFFFFFF); }

        uint64_t rawId = 0;
    };

    constexpr EffectHandle InvalidEffectHandle{};

    /// @brief Defines the frequency response shape of a standard biquadratic filter.
    enum class BiquadFilterType {
        LowPass,	// Allows frequencies below the cutoff frequency to pass, attenuating higher frequencies.
        HighPass,	// Allows frequencies above the cutoff frequency to pass, attenuating lower frequencies.
        BandPass	// Allows a specific range of frequencies to pass, attenuating frequencies outside the band.
    };

    struct BiquadConfig {
        float frequency = 20000.0f; // The cutoff or center frequency of the filter (in Hz).
        float resonance = 0.707f;	// The Q-factor of the filter.
    };

}