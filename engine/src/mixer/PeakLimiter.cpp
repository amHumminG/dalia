#include "mixer/PeakLimiter.h"

#include <cmath>
#include <algorithm>

void PeakLimiter::Init(float sampleRate, float attackMs, float releaseMs) {
	m_attackCoeff = std::exp(-1.0f / ((attackMs * 0.001f) * sampleRate));
	m_releaseCoeff = std::exp(-1.0f / ((releaseMs * 0.001f) * sampleRate));
}

void PeakLimiter::ProcessBuffer(float* buffer, uint32_t frameCount, uint32_t channels) {
	for (uint32_t i = 0; i < frameCount; i++) {
		uint32_t frameIndex = i * channels;

		// Find the loudest peak among all channels in this frame
		float maxPeak = 0.0f;
		for (uint32_t c = 0; c < channels; c++) {
			maxPeak = std::max(maxPeak, std::abs(buffer[frameIndex + c]));
		}

		// Track peak volume
		if (maxPeak > m_envelope) {
			m_envelope = m_attackCoeff * m_envelope + (1.0f - m_attackCoeff) * maxPeak;
		}
		else {
			m_envelope = m_releaseCoeff * m_envelope + (1.0f - m_releaseCoeff) * maxPeak;
		}

		// Calculate gain reduction
		float targetGain = 1.0f;
		if (m_envelope > m_threshold) targetGain = m_threshold / m_envelope;

		// Smooth gain application
		if (targetGain < m_currentGain) {
			m_currentGain = m_attackCoeff * m_currentGain + (1.0f - m_attackCoeff) * targetGain;
		}
		else {
			m_currentGain = m_releaseCoeff * m_currentGain + (1.0f - m_releaseCoeff) * targetGain;
		}

		// Apply gain
		for (uint32_t c = 0; c < channels; c++) {
			buffer[frameIndex + c] *= m_currentGain;
		}
	}
}
