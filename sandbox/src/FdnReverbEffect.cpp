#include "FdnReverbEffect.hpp"

#include <imgui.h>

FdnReverbEffect::FdnReverbEffect(dalia::Engine* engine, const std::string& name) {
	m_engine = engine;
	m_name   = name;

	m_result = engine->CreateEffect(m_handle, m_config);
	if (m_result != dalia::Result::Ok) return;
}

void FdnReverbEffect::DrawInspectorUI(const UIContext& ui) {
	ImGui::PushID(this);

	ImGui::PushFont(ui.headerFont);
	ImGui::Text("[E] %s", m_name.c_str());
	ImGui::PopFont();

	ImGui::Text("Result: ");
	ImGui::SameLine();
	if (m_result == dalia::Result::Ok) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetResultString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetResultString(m_result));
	}

	ImGui::Spacing();

	// Parameters
	bool paramsChanged = false;

	if (ImGui::SliderFloat("Decay Time", &m_config.decayTimeSeconds, 0.1f, 15.0f, "%.2f s")) {
		paramsChanged = true;
	}

	if (ImGui::SliderFloat(
	        "Damping Frequency", &m_config.dampingHz, 500.0f, 20000, "%.0f Hz", ImGuiSliderFlags_Logarithmic)) {
		paramsChanged = true;
	}

	if (ImGui::SliderFloat("Pre-Delay", &m_config.preDelayMs, 0.0f, 100.0f, "%.0f ms")) {
		paramsChanged = true;
	}

	if (ImGui::SliderFloat("Dry Mix", &m_config.dryMix, 0.0f, 1.0f, "%.2f")) {
		paramsChanged = true;
	}

	if (ImGui::SliderFloat("Wet Mix", &m_config.wetMix, 0.0f, 1.0f, "%.2f")) {
		paramsChanged = true;
	}

	if (ImGui::SliderFloat("Low Cut Frequency", &m_config.lowCutHz, 20.0f, 500.0f, "%.0f")) {
		paramsChanged = true;
	}

	if (ImGui::Checkbox("Freeze", &m_config.freeze)) {
		paramsChanged = true;
	}

	if (paramsChanged) m_result = m_engine->SetEffectParams(m_handle, m_config);

	ImGui::PopID();
}
