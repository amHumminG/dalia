#include "BiquadEffect.h"

#include "imgui.h"

BiquadEffect::BiquadEffect(dalia::Engine* engine, const std::string& name) {
	m_engine = engine;
	m_name = name;

	m_result = engine->CreateEffect(m_handle, m_config);
	if (m_result != dalia::Result::Ok) return;
}

void BiquadEffect::DrawInspectorUI(const UIContext& ui) {
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

	int filterTypeInt = static_cast<int>(m_config.type);
	if (ImGui::RadioButton("LowPass", &filterTypeInt, 0) ||
		ImGui::RadioButton("HighPass", &filterTypeInt, 1) ||
		ImGui::RadioButton("BandPass", &filterTypeInt, 2)) {
		m_config.type = static_cast<dalia::BiquadFilterType>(filterTypeInt);
		paramsChanged = true;
	}

	if (ImGui::SliderFloat("Frequency", &m_config.frequency, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic)) {
		paramsChanged = true;
	}

	if (ImGui::SliderFloat("Resonance", &m_config.resonance, 0.1f, 10.0f, "%.2f")) {
		paramsChanged = true;
	}

	if (paramsChanged) m_result = m_engine->SetEffectConfig(m_handle, m_config);

	ImGui::PopID();
}
