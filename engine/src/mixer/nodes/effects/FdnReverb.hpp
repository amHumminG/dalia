#pragma once

#include "dalia/Effect.hpp"
#include "mixer/nodes/Effect.hpp"
#include "core/Constants.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dalia {

	struct FdnReverb {
	public:
		static constexpr size_t TANK_CHANNELS    = 8; // Must be power of 2
		static constexpr size_t DIFFUSION_STAGES = 4;

		static constexpr std::array<float, DIFFUSION_STAGES> DIFFUSION_STAGE_MS = {7.0f, 15.0f, 31.0f, 63.0f};
		static constexpr float TANK_MIN_DELAY_MS                                = 45.0f;

		uint32_t gen = NO_GENERATION;

		void SetParams(const FdnReverbParams& params);
		void Prepare(float sampleRate);
		void Process(float* buffer, uint32_t frameCount, uint32_t channels);
		void Flush();
		void Reset();

	private:
		struct DelayLine {
			std::vector<float> buffer;
			size_t writeIndex = 0;

			float Read() { return buffer[writeIndex]; }

			void Write(float sample) {
				buffer[writeIndex] = sample;
				if (++writeIndex >= buffer.size()) writeIndex = 0;
			}

			void Clear() {
				std::fill(buffer.begin(), buffer.end(), 0.0f);
				writeIndex = 0;
			}
		};

		struct DiffusionStage {
			std::array<DelayLine, TANK_CHANNELS> delays{};
			std::array<uint8_t, TANK_CHANNELS> permutation{};
			std::array<float, TANK_CHANNELS> polarity{};

			void Prepare(float sampleRate, float delayRangeMs, uint32_t seed);
			void Process(std::array<float, TANK_CHANNELS>& data);
			void Clear();
		};

		static void HadamardInPlace(std::array<float, TANK_CHANNELS>& data);
		static void HouseholderInPlace(std::array<float, TANK_CHANNELS>& data);
		void ProcessPreDelay(std::array<float, TANK_CHANNELS>& data);

		float m_sampleRate = 48000.0f;
		FdnReverbParams m_params{};

		std::array<DelayLine, TANK_CHANNELS> m_preDelays{};
		size_t m_preDelaySamples = 0;

		std::array<DiffusionStage, DIFFUSION_STAGES> m_diffusers{};

		std::array<DelayLine, TANK_CHANNELS> m_tankDelays{};
		std::array<float, TANK_CHANNELS> m_feedbackGains{};
		std::array<float, TANK_CHANNELS> m_dampingState{};
		std::array<float, TANK_CHANNELS> m_lowCutState{};
		float m_dampingCoeff = 0.0f;
		float m_lowCutCoeff = 0.0f;
	};

	struct FdnReverbMirror : EffectMirror {
		FdnReverbParams params{};

		void Reset() {
			ResetBase();
			params = FdnReverbParams{};
		}
	};
}
