#pragma once

#include "core/Constants.h"

#include <cstdint>
#include <vector>

struct PeakLimiter {
	float m_envelope = 0.0f;
	float m_currentGain = 1.0f;

	float m_attackCoeff = 0.0f;
	float m_releaseCoeff = 0.0f;
	float m_threshold = 0.9f;

	void Init(float sampleRate, float attackMs = 2.0f, float releaseMs = 50.0f);

	void ProcessBuffer(float* DALIA_RESTRICT buffer, uint32_t frameCount, uint32_t channels);
};