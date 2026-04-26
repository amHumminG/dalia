#include "Listener.h"

Listener::Listener(dalia::Engine* engine, uint32_t index, bool isActive)
	: m_engine(engine), m_index(index), m_isActive(isActive) {
}

void Listener::Update() {
	if (!m_isActive) return;

	m_position.x += m_velocity.x * GetFrameTime();
	m_position.y += m_velocity.y * GetFrameTime();
	m_position.z += m_velocity.z * GetFrameTime();

	dalia::Listener3DAttributes attributes;
	attributes.position = {m_position.x, m_position.y, m_position.z};
	attributes.distanceProbePosition = {m_ProbePosition.x, m_ProbePosition.y, m_ProbePosition.z};
	attributes.forward = {m_forward.x, m_forward.y, m_forward.z};
	attributes.up = {m_up.x, m_up.y, m_up.z};
	attributes.velocity = {m_velocity.x, m_velocity.y, m_velocity.z};

	dalia::Result res = m_engine->SetListener3DAttributes(m_index, attributes);
	if (res != dalia::Result::Ok) m_result = res;
}

void Listener::Draw3D(bool isSelected) {
	if (!m_isActive) return;

	Color listenerColor = SKYBLUE;

	DrawSphere(m_position, 0.5f, Fade(listenerColor, 0.8f));

	// Orientation
	float vecLength = 1.5f;
	Vector3 forwardVec = {
		m_position.x + (m_forward.x * vecLength),
		m_position.y + (m_forward.y * vecLength),
		m_position.z + (m_forward.z * vecLength),
	};

	Vector3 upVec = {
		m_position.x + (m_up.x * vecLength),
		m_position.y + (m_up.y * vecLength),
		m_position.z + (m_up.z * vecLength),
	};

	DrawLine3D(m_position, forwardVec, BLUE);
	DrawLine3D(m_position, upVec, GREEN);

	// Draw distance probe (if used)
	if (Vector3Distance(m_position, m_ProbePosition) > 0.01f) {
		DrawSphere(m_ProbePosition, 0.2f, ORANGE);
		DrawLine3D(m_position, m_ProbePosition, Fade(ORANGE, 0.5f));
	}

	if (isSelected) DrawSphereWires(m_position, 0.6f, 8, 8, WHITE);
	else DrawSphereWires(m_position, 0.6f, 8, 8, DARKGRAY);
}

void Listener::DrawInspectorUI(const UIContext& ui) {
	ImGui::PushID(this);

	ImGui::PushFont(ui.headerFont);
	ImGui::Text("[L] %u", m_index);
	ImGui::PopFont();
	ImGui::Separator();

	dalia::Result res;

	if (ImGui::Checkbox("Active", &m_isActive)) {
		res = m_engine->SetListenerActive(m_index, m_isActive);
		if (res != dalia::Result::Ok) m_result = res;
	}

	if (m_isActive) {
		ImGui::Indent();

		// Pilot Controls
		ImGui::Separator();
		ImGui::TextDisabled("Camera Control");

		if (isPiloted) {
			ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.2f, 0.2f, 1.0f});
			if (ImGui::Button("Stop Piloting")) isPiloted = false;
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::Text("Driving:");
			ImGui::SameLine();

			int* target = reinterpret_cast<int*>(&m_pilotTarget);
			ImGui::RadioButton("Head", target, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Probe", target, 1);
		}
		else {
			if (ImGui::Button("Pilot Listener")) isPiloted = true;
		}

		ImGui::TextDisabled("Transform");

		if (isPiloted) ImGui::BeginDisabled();

		// TODO: Add transform setters for position and velocity

		ImGui::Separator();

		ImGui::TextDisabled("Advanced");

		// TODO: Add position setter for probe position

		if (isPiloted) ImGui::EndDisabled();

		ImGui::Unindent();
	}

	ImGui::PopID();
}

void Listener::SyncWithCamera(const Camera3D& camera) {
	if (m_pilotTarget == PilotTarget::Head) {
		m_position = camera.position;
		m_up = camera.up;
		m_forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
	}
	else if (m_pilotTarget == PilotTarget::Probe) {
		m_ProbePosition = camera.position;
	}
}
