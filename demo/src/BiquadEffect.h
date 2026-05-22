#pragma once

#include "Effect.h"

class BiquadEffect : public Effect {
public:
	BiquadEffect(dalia::Engine* engine, const std::string& name, dalia::BiquadFilterType filterType);

	void DrawInspectorUI(const UIContext& ui) override;

	dalia::BiquadFilterType GetFilterType() const { return m_filterType; }

private:
	dalia::BiquadFilterType m_filterType;
	dalia::BiquadConfig m_config;

};