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
		static constexpr std::array<float, CHANNELS_MAX> BASE_DELAY_MS = {
			29.7f, 37.1f, 41.3f, 43.7f, 53.1f, 59.3f, 67.7f, 73.3f
		};

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
		};

		float ReadDelay(const DelayLine& line) { return line.buffer[line.writeIndex]; }
		void WriteDelay(DelayLine& line, float sample);
		float ProcessPreDelay(float input);

		float m_sampleRate = 48000;
		FdnReverbParams m_params;

		DelayLine m_preDelay{};
		size_t m_preDelaySamples = 0;

		std::array<DelayLine, CHANNELS_MAX> m_delays{};
		std::array<float, CHANNELS_MAX> m_feedbackGains{};
		std::array<float, CHANNELS_MAX> m_dampingState{};
		float m_dampingCoeff = 0.0f;
	};

	struct FdnReverbMirror : EffectMirror {
		FdnReverbParams params{};

		void Reset() {
			ResetBase();
			params = FdnReverbParams{};
		}
	};
}
