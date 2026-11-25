#include "mixer/nodes/effects/FdnReverb.hpp"

#include <algorithm>
#include <numbers>

namespace dalia {

    void FdnReverb::SetParams(const FdnReverbParams& params)
    {
        m_params = params;
        if (m_preDelay.buffer.empty()) return; // Underflow protection

        // Pre delay length in samples
        size_t maxPre = m_preDelay.buffer.size() - 1;
        m_preDelaySamples = std::min(static_cast<size_t>(params.preDelayMs * 0.001f * m_sampleRate), maxPre);

        // Per-channel T60 feedback gains: g_i = 10^(-3 * delaySeconds / T60)
        const float t60 = std::max(params.decayTimeSeconds, 0.05f);
        for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
            if (params.freeze) m_feedbackGains[c] = 1.0f;
            else
            {
                const float delaySec = static_cast<float>(m_delays[c].buffer.size() / m_sampleRate);
                m_feedbackGains[c] = std::pow(10.0f, (-3.0f * delaySec) / t60);
            }
        }

        // One-pole lowpass damping coefficient: y[n] = (1-a) * x[n] + a * y[n-1]
        const float fc = std::clamp(params.dampingHz, 100.0f, m_sampleRate * 0.45f);
        m_dampingCoeff = std::exp(-2.0f * std::numbers::pi_v<float> * fc / m_sampleRate);
    }

    void FdnReverb::Prepare(float sampleRate) {
        m_sampleRate = sampleRate;

        constexpr float MAX_PRE_DELAY_MS = 100.0f;
        size_t maxPreDelaySamples = static_cast<size_t>(std::ceil(MAX_PRE_DELAY_MS * 0.001f * m_sampleRate)) + 1;
        m_preDelay.buffer.assign(maxPreDelaySamples, 0.0f);
        m_preDelay.writeIndex = 0;

        for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
            size_t length = static_cast<size_t>(std::round(BASE_DELAY_MS[c] * 0.001f * m_sampleRate));
            m_delays[c].buffer.assign(std::max<size_t>(length, 1), 0.0f);
            m_delays[c].writeIndex = 0;
        }

        Flush();
        SetParams(m_params);
    }

    void FdnReverb::Process(float* buffer, uint32_t frameCount, uint32_t channels) {
        if (m_preDelay.buffer.empty() || channels == 0) return;

        float invChannels = 1.0f / static_cast<float>(channels);
        constexpr float HOUSEHOLDER_FACTOR = 2.0f / static_cast<float>(CHANNELS_MAX);

        for (uint32_t f = 0; f < frameCount; f++) {
            float* frame = buffer + (f * channels);

            // Sum input channels to mono and run through pre-delay
            float monoIn = 0.0f;
            for (uint32_t c = 0; c < channels; c++) monoIn += frame[c];
            monoIn *= invChannels;

            float tankIn = 0.0f;
            if (m_params.freeze) m_dampingCoeff = 0.0f;
            else tankIn = ProcessPreDelay(monoIn);

            // Read all 8 tank delay lines, apply one-pole lowpass damping & T60 gain
            std::array<float, CHANNELS_MAX> delayed{};
            float sum = 0.0f;

            for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
                float sample = ReadDelay(m_delays[c]);

                sample = (1.0f - m_dampingCoeff) * sample + m_dampingCoeff * m_dampingState[c];
                m_dampingState[c] = sample;

                sample *= m_feedbackGains[c];
                delayed[c] = sample;
                sum += sample;
            }

            // O(N) Householder reflection and write-back into delay lines
            float householderScale = sum * HOUSEHOLDER_FACTOR;
            for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
                float inputSign = (c % 2 == 0) ? 1.0f : -1.0f;
                float feedbackSample = (delayed[c] - householderScale) + (tankIn * inputSign * 0.25f);
                WriteDelay(m_delays[c], feedbackSample);
            }

            // Mix wet/dry back into the output frame
            if (channels == CHANNELS_MONO) {
                float wet = sum * (1.0f / static_cast<float>(CHANNELS_MAX));
                frame[0] = frame[0] * m_params.dryMix + wet * m_params.wetMix;
            }
            else if (channels == CHANNELS_STEREO) {
                float wetL = (delayed[0] + delayed[2] + delayed[4] + delayed[6]) * 0.25f;
                float wetR = (delayed[1] + delayed[3] + delayed[5] + delayed[7]) * 0.25f;
                frame[0] = frame[0] * m_params.dryMix + wetL * m_params.wetMix;
                frame[1] = frame[1] * m_params.dryMix + wetR * m_params.wetMix;
            }
            else {
                for (uint32_t c = 0; c < channels; c++) {
                    float wet = delayed[c % CHANNELS_MAX] * 0.5f;
                    frame[c] = frame[c] * m_params.dryMix + wet * m_params.wetMix;
                    // TODO: Skip or lowpass reverb for the LFE channel
                }
            }
        }
    }

    void FdnReverb::Flush()
    {
        std::fill(m_preDelay.buffer.begin(), m_preDelay.buffer.end(), 0.0f);
        m_preDelay.writeIndex = 0;

        for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
            std::fill(m_delays[c].buffer.begin(), m_delays[c].buffer.end(), 0.0f);
            m_delays[c].writeIndex = 0;
            m_dampingState[c] = 0.0f;
        }
    }

    void FdnReverb::Reset() {
        gen++;

        Flush();
        SetParams(FdnReverbParams{});
    }

    void FdnReverb::WriteDelay(DelayLine& line, float sample) {
        line.buffer[line.writeIndex] = sample;
        if (++line.writeIndex >= line.buffer.size()) line.writeIndex = 0;
    }

    float FdnReverb::ProcessPreDelay(float input) {
        if (m_preDelaySamples == 0)  return input;

        size_t bufSize = m_preDelay.buffer.size();
        size_t readIndex = (m_preDelay.writeIndex + bufSize - m_preDelaySamples) % bufSize;
        float output = m_preDelay.buffer[readIndex];

        m_preDelay.buffer[m_preDelay.writeIndex] = input;
        if (++m_preDelay.writeIndex >= bufSize) m_preDelay.writeIndex = 0;

        return output;
    }
}
