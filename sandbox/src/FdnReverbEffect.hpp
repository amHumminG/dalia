#pragma once

#include "Effect.hpp"

class FdnReverbEffect : public Effect {
public:
	FdnReverbEffect(dalia::Engine* engine, const std::string& name);

	void DrawInspectorUI(const UIContext& ui) override;

private:
	dalia::FdnReverbParams m_config;
};
