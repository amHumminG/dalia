#pragma once

#include "Effect.h"

class BiquadEffect : public Effect {
public:
	BiquadEffect(dalia::Engine* engine, const std::string& name);

	void DrawInspectorUI(const UIContext& ui) override;

private:
	dalia::BiquadParams m_config;

};