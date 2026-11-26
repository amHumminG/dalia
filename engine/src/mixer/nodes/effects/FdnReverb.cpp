#include "mixer/nodes/effects/FdnReverb.hpp"

#include <algorithm>
#include <numbers>
#include <random>
#include <cmath>
#include <numeric>
#include <bit>

namespace dalia {

	namespace {
		constexpr float INV_SQRT_8 = std::numbers::sqrt2 * 0.25f;
	}

	// --- Matrix Operations ---

	void FdnReverb::HadamardInPlace(std::array<float, TANK_CHANNELS>& data) {
		static_assert(std::has_single_bit(TANK_CHANNELS), "TANK_CHANNELS must be power of 2");

		for (size_t h = 1; h < TANK_CHANNELS; h <<= 1) {
			for (size_t i = 0; i < TANK_CHANNELS; i += h * 2) {
				for (size_t j = i; j < i + h; j++) {
					float a     = data[j];
					float b     = data[j + h];
					data[j]     = a + b;
					data[j + h] = a - b;
				}
			}
		}

		for (float& x : data) x *= INV_SQRT_8;
	}

	void FdnReverb::HouseholderInPlace(std::array<float, TANK_CHANNELS>& data) {
		float sum = 0.0f;
		for (float x : data) sum += x;

		constexpr float FACTOR = 2.0f / static_cast<float>(TANK_CHANNELS);
		float offset           = sum * FACTOR;
		for (float& x : data) x -= offset;
	}

	// --- Diffuser ---

	void FdnReverb::DiffusionStage::Prepare(float sampleRate, float delayRangeMs, uint32_t seed) {
		std::mt19937 rng(seed);
		float rangeSamples = delayRangeMs * 0.001f * sampleRate;

		// Stratified random delay
		// Each channel c picks randomly inside [range * c/N, range * (c+1)/N]
		for (size_t c = 0; c < TANK_CHANNELS; c++) {
			size_t low = static_cast<size_t>(rangeSamples * static_cast<float>(c) / static_cast<float>(TANK_CHANNELS));
			size_t high =
			    static_cast<size_t>(rangeSamples * static_cast<float>(c + 1) / static_cast<float>(TANK_CHANNELS));
			std::uniform_int_distribution<size_t> distribution(low, std::max(low, high));

			size_t length = std::max<size_t>(distribution(rng), 1);
			delays[c].buffer.assign(length, 0.0f);
			delays[c].writeIndex = 0;
		}

		// Channel shuffle
		std::iota(permutation.begin(), permutation.end(), uint8_t{0});
		std::ranges::shuffle(permutation, rng);

		// Polarity flips (balanced) + shuffle
		for (size_t c = 0; c < TANK_CHANNELS; c++) polarity[c] = (c % 2 == 0) ? 1.0f : -1.0f;
		std::ranges::shuffle(polarity, rng);
	}

	void FdnReverb::DiffusionStage::Process(std::array<float, TANK_CHANNELS>& data) {
		std::array<float, TANK_CHANNELS> delayed{};
		for (size_t c = 0; c < TANK_CHANNELS; c++) {
			delayed[c] = delays[c].Read();
			delays[c].Write(data[c]);
		}

		for (size_t c = 0; c < TANK_CHANNELS; c++) data[c] = delayed[permutation[c]] * polarity[c];

		HadamardInPlace(data);
	}

	void FdnReverb::DiffusionStage::Clear() {
		for (auto& d : delays) d.Clear();
	}

	// --- FDN Reverb ---

	void FdnReverbParams::Sanitize() {
		decayTimeSeconds = std::clamp(decayTimeSeconds, REVERB_DECAY_TIME_SECONDS_MIN, REVERB_DECAY_TIME_SECONDS_MAX);
		dampingHz        = std::clamp(dampingHz, REVERB_DAMPING_FREQUENCY_MIN, REVERB_DAMPING_FREQUENCY_MAX);
		preDelayMs       = std::clamp(preDelayMs, REVERB_PRE_DELAY_MILLISECONDS_MIN, REVERB_PRE_DELAY_MILLISECONDS_MAX);
		wetMix           = std::clamp(wetMix, MIX_MIN, MIX_MAX);
		dryMix           = std::clamp(dryMix, MIX_MIN, MIX_MAX);
		lowCutHz         = std::clamp(lowCutHz, REVERB_LOW_CUT_FREQUENCY_MIN, REVERB_LOW_CUT_FREQUENCY_MAX);
	}

	void FdnReverb::SetParams(const FdnReverbParams& params) {
		m_params = params;
		if (m_tankDelays[0].buffer.empty()) return; // Underflow protection

		// Pre delay length in samples
		size_t maxPre     = m_preDelays[0].buffer.size() - 1;
		m_preDelaySamples = std::min(static_cast<size_t>(params.preDelayMs * 0.001f * m_sampleRate), maxPre);

		// Per-channel T60 feedback gains: g_i = 10^(-3 * delaySeconds / T60)
		float t60 = std::max(params.decayTimeSeconds, 0.05f);
		for (uint32_t c = 0; c < TANK_CHANNELS; c++) {
			if (params.freeze)
				m_feedbackGains[c] = 1.0f;
			else {
				float delaySec     = static_cast<float>(m_tankDelays[c].buffer.size() / m_sampleRate);
				m_feedbackGains[c] = std::pow(10.0f, (-3.0f * delaySec) / t60);
			}
		}

		// One-pole lowpass damping coefficient: y[n] = (1-a) * x[n] + a * y[n-1]
		float dampFc   = std::clamp(m_params.dampingHz, 100.f, m_sampleRate * 0.45f);
		m_dampingCoeff = std::exp(-2.0f * std::numbers::pi_v<float> * dampFc / m_sampleRate);

		// One-pole highpass low-cut coefficients
		float lowCutFc = std::clamp(params.lowCutHz, 10.0f, m_sampleRate * 0.25f);
		m_lowCutCoeff  = std::exp(-2.0f * std::numbers::pi_v<float> * lowCutFc / m_sampleRate);
	}

	void FdnReverb::Prepare(float sampleRate) {
		m_sampleRate = sampleRate;

		size_t maxPreDelaySamples =
		    static_cast<size_t>(std::ceil(REVERB_PRE_DELAY_MILLISECONDS_MAX * 0.001f * m_sampleRate)) + 1;
		for (auto& pre : m_preDelays) {
			pre.buffer.assign(maxPreDelaySamples, 0.0f);
			pre.writeIndex = 0;
		}

		// Diffusers (deterministic seeds for consistency)
		for (size_t s = 0; s < DIFFUSION_STAGES; ++s) {
			m_diffusers[s].Prepare(m_sampleRate, DIFFUSION_STAGE_MS[s], 0xDA11A001u + static_cast<uint32_t>(s));
		}

		// Feedback tank (exponentially spaced delays)
		float baseSamples = TANK_MIN_DELAY_MS * 0.001f * m_sampleRate;
		for (size_t c = 0; c < TANK_CHANNELS; c++) {
			float exponent = static_cast<float>(c) / static_cast<float>(TANK_CHANNELS);
			size_t length  = static_cast<size_t>(std::round(baseSamples * std::pow(2.0f, exponent)));
			m_tankDelays[c].buffer.assign(std::max<size_t>(length, 1), 0.0f);
			m_tankDelays[c].writeIndex = 0;
		}

		Flush();
		SetParams(m_params);
	}

	void FdnReverb::Process(float* buffer, uint32_t frameCount, uint32_t channels) {
		if (m_tankDelays[0].buffer.empty() || channels == 0) return;

		for (uint32_t f = 0; f < frameCount; f++) {
			float* frame = buffer + (f * channels);

			// Upmix input to TANK_CHANNELS
			// 0.5x give 5dB tank headroom while keeping mono/stereo/surround equal
			std::array<float, TANK_CHANNELS> diffused{};
			if (!m_params.freeze) {
				if (channels == CHANNELS_MONO) {
					diffused.fill(frame[0] * 0.5f);
				}
				else if (channels == CHANNELS_STEREO) {
					for (size_t c = 0; c < TANK_CHANNELS; c += 2) {
						diffused[c]     = frame[0] * 0.5f;
						diffused[c + 1] = frame[1] * 0.5f;
					}
				}
				else {
					for (size_t c = 0; c < TANK_CHANNELS; c++) {
						diffused[c] = frame[c % channels] * 0.5f;
					}
				}

				// Pre-delay + DIFFUSION_STAGES Hadamard diffuser
				ProcessPreDelay(diffused);
				for (size_t s = 0; s < DIFFUSION_STAGES; s++) m_diffusers[s].Process(diffused);
			}

			// Read feedback tank delays and apply damping, low-cut and T60 gains
			std::array<float, TANK_CHANNELS> tankOut{};
			std::array<float, TANK_CHANNELS> mixed{};

			for (size_t c = 0; c < TANK_CHANNELS; c++) {
				float sample = m_tankDelays[c].Read();

				if (!m_params.freeze) {
					// One-pole lowpass damping (high frequency absorption)
					sample            = (1.0f - m_dampingCoeff) * sample + m_dampingCoeff * m_dampingState[c];
					m_dampingState[c] = sample;

					// One-pole low-shelf (prevent sub-bass buildup tail buildup)
					constexpr float LOW_CUT_STRENGTH = 0.35f;
					m_lowCutState[c] = (1.0f - m_lowCutCoeff) * sample + m_lowCutCoeff * m_lowCutState[c];
					sample -= m_lowCutState[c] * LOW_CUT_STRENGTH;
				}

				tankOut[c] = sample;
				mixed[c]   = sample * m_feedbackGains[c];
			}

			// Householder feedback matrix
			HouseholderInPlace(mixed);

			// Write back into tank delays
			for (size_t c = 0; c < TANK_CHANNELS; c++) m_tankDelays[c].Write(diffused[c] + mixed[c]);

			// Downmix TANK_CHANNELS into output (1 / sqrt(K) preserves RMS across different speaker layouts)
			if (channels == CHANNELS_MONO) {
				float sum = 0.0f;
				for (float x : tankOut) sum += x;
				float wet = sum * INV_SQRT_8; // K = 8 -> 1 / sqrt(8)
				frame[0]  = frame[0] * m_params.dryMix + wet * m_params.wetMix;
			}
			else if (channels == CHANNELS_STEREO) {
				float wetL = (tankOut[0] + tankOut[2] + tankOut[4] + tankOut[6]) * 0.5f; // K = 4 -> 1 / sqrt(4)
				float wetR = (tankOut[1] + tankOut[3] + tankOut[5] + tankOut[7]) * 0.5f; // K = 4 -> 1 / sqrt(4)
				frame[0]   = frame[0] * m_params.dryMix + wetL * m_params.wetMix;
				frame[1]   = frame[1] * m_params.dryMix + wetR * m_params.wetMix;
			}
			else {
				for (uint32_t c = 0; c < channels; c++) {
					float wet = tankOut[c % TANK_CHANNELS]; // K = 1 -> 1.0f
					frame[c]  = frame[c] * m_params.dryMix + wet * m_params.wetMix;
					// TODO: Skip or lowpass reverb for the LFE channel
				}
			}
		}
	}

	void FdnReverb::ProcessPreDelay(std::array<float, TANK_CHANNELS>& data) {
		if (m_preDelaySamples == 0) return;

		size_t bufSize = m_preDelays[0].buffer.size();
		for (size_t c = 0; c < TANK_CHANNELS; c++) {
			DelayLine& line  = m_preDelays[c];
			size_t readIndex = (line.writeIndex + bufSize - m_preDelaySamples) % bufSize;
			float output     = line.buffer[readIndex];
			line.Write(data[c]);
			data[c] = output;
		}
	}

	void FdnReverb::Flush() {
		for (auto& pre : m_preDelays) pre.Clear();
		for (auto& diffuser : m_diffusers) diffuser.Clear();
		for (auto& tank : m_tankDelays) tank.Clear();

		m_dampingState.fill(0.0f);
		m_lowCutState.fill(0.0f);
	}

	void FdnReverb::Reset() {
		gen++;

		Flush();
		SetParams(FdnReverbParams{});
	}
}
