#pragma once

/// @file Effect.hpp
/// @brief DSP effect definitions, handles, and parameter structures.

#include <cstdint>

namespace dalia {
	/// @brief Defines the available DSP effect algorithms.
	enum class EffectType : uint8_t {
		None = 0,		///< Sentinel value.
		Biquad = 1,		///< Biquadratic IIR filter.
		FdnReverb = 2	///< Feedback Delay Network Reverb
	};

	/// @brief Handle used to manage effect instances. This handle expires once the effect it is referencing has been
	/// destroyed.
	struct EffectHandle {
	public:
		EffectHandle() = default;

		explicit EffectHandle(uint64_t rawId)
			: m_rawId(rawId) {
		}

		/// @return true if the handle has referenced an effect at some point. Otherwise, false.
		bool IsValid() const {
			return m_rawId != 0;
		}

		bool operator==(const EffectHandle& other) const {
			return m_rawId == other.m_rawId;
		}

		bool operator!=(const EffectHandle& other) const {
			return m_rawId != other.m_rawId;
		}

		/// @return The underlying raw id of the handle.
		uint64_t GetRawId() const {
			return m_rawId;
		}

		/// @return The type of effect the handle is referencing.
		EffectType GetType() const {
			return static_cast<EffectType>(m_rawId >> 56);
		}

	private:
		friend class Engine;
		friend class MixController;
		friend class Mixer;
		friend class BusProcessor;
		friend class EffectProcessor;

		static EffectHandle Create(uint32_t index, uint32_t generation, EffectType type) {
			EffectHandle handle;
			uint64_t typeBits = static_cast<uint64_t>(type) << 56;
			uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
			uint64_t indexBits = static_cast<uint64_t>(index);

			handle.m_rawId = typeBits | generationBits | indexBits;
			return handle;
		}

		static EffectHandle FromRawId(uint64_t rawId) {
			EffectHandle handle;
			handle.m_rawId = rawId;
			return handle;
		}

		uint32_t GetIndex() const {
			return static_cast<uint32_t>(m_rawId & 0xFFFFFFFF);
		}

		uint32_t GetGeneration() const {
			return static_cast<uint32_t>((m_rawId >> 32) & 0xFFFFFF);
		}

		uint64_t m_rawId = 0;
	};

	/// @brief Sentinel value representing an invalid effect instance.
	constexpr EffectHandle INVALID_EFFECT_HANDLE{};

	/// @brief Configuration parameters for a biquadratic IIR filter.
	struct BiquadParams {
		/// @brief Defines the frequency response shape of a standard biquadratic filter.
		enum class Type : uint8_t {
			LowPass  = 0,	///< Allows frequencies below the cutoff frequency to pass, attenuating higher frequencies.
			HighPass = 1,	///< Allows frequencies above the cutoff frequency to pass, attenuating lower frequencies.
			BandPass = 2	///< Allows a specific range of frequencies to pass, attenuating frequencies outside the band.
		} type = Type::LowPass;

		float frequency = 20000.0f; ///< The cutoff or center frequency of the filter: 20.0 - 20000 Hz
		float resonance = 0.707f;	///< The Q-factor of the filter: 0.1 - 10.0

		/// @brief Clamps internal parameters to safe ranges.
		void Sanitize();
	};

	/// @brief Configuration parameters for a feedback delay network (FDN) reverb.
	struct FdnReverbParams {
		float decayTimeSeconds	= 4.0f;		///< Time for reverb tail to decay by 60 dB (T60): 0.1 - 15.0 s
		float roomSize			= 0.6f;		///< Virtual space scale controlling echo spacing: 0.25 - 1.0
		float dampingHz			= 2500.0f;	///< High-frequency damping lowpass cutoff: 500 - 20000 Hz
		float preDelayMs		= 20.0f;	///< Delay before reflections begin: 0 - 100 ms
		float dryMix			= 1.0f;		///< Linear gain of the unprocessed input signal: 0.0 - 1.0
		float wetMix			= 0.35f;	///< Linear gain of the processed reverb signal: 0.0 - 1.0
		float lowCutHz			= 80.0f;	///< Sub-bass buildup low-shelf cutoff: 20 - 500 Hz
		float modulation		= 0.5f;		///< Tank delay LFO modulation depth: 0.0 - 1.0
		bool freeze				= false;	///< Sustains the current tail infinitely without new input: true/false

		/// @brief Clamps internal parameters to safe ranges.
		void Sanitize();
	};
}
