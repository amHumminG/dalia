#include "MixingBus.h"

#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

MixingBus::MixingBus(dalia::Engine* engine, const std::string& identifier, const std::string& parentIdentifier)
	: m_engine(engine), m_identifier(identifier), m_parentIdentifier(parentIdentifier) {
	if (m_identifier == "Master") m_parentIdentifier = "None";
	else m_result = m_engine->CreateBus(m_identifier.c_str(), m_parentIdentifier.c_str());

	if (m_result == dalia::Result::Ok) {
		std::strncpy(m_routeInputBuffer, m_parentIdentifier.c_str(), sizeof(m_routeInputBuffer) - 1);
	}
}

MixingBus::~MixingBus() {
	m_engine->DestroyBus(m_identifier.c_str());
}

void MixingBus::DrawInspectorUI() {
	ImGui::PushID(this);

	ImGui::TextColored({0.8f, 0.4f, 1.0f, 1.0f}, "Mixing Bus: %s", m_identifier.c_str());

	ImGui::Separator();

	ImGui::Text("Result: ");
	ImGui::SameLine();
	if (m_result == dalia::Result::Ok) {
		ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}
	else {
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, dalia::GetErrorString(m_result));
	}

	ImGui::Separator();

	ImGui::TextDisabled("Routing Topology");

	if (m_identifier == "Master") {
		ImGui::Text("Parent: None (Root Node)");
	}
	else {
		ImGui::Text("Parent: %s", m_parentIdentifier.c_str());

		ImGui::InputText("New Parent", m_routeInputBuffer, sizeof(m_routeInputBuffer));
		ImGui::SameLine();

		if (ImGui::Button("Route")) {
			std::string newParent(m_routeInputBuffer);

			dalia::Result res = m_engine->RouteBus(m_identifier.c_str(), newParent.c_str());

			if (res == dalia::Result::Ok) {
				m_parentIdentifier = newParent;
			}
			else {
				std::strncpy(m_routeInputBuffer, m_parentIdentifier.c_str(), sizeof(m_routeInputBuffer) - 1);
			}
		}
	}

	ImGui::PopID();
}




