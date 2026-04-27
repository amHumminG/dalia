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
	attributes.distanceProbePosition = {m_probePosition.x, m_probePosition.y, m_probePosition.z};
	attributes.forward = {m_forward.x, m_forward.y, m_forward.z};
	attributes.up = {m_up.x, m_up.y, m_up.z};
	attributes.velocity = {m_velocity.x, m_velocity.y, m_velocity.z};

	dalia::Result res = m_engine->SetListener3DAttributes(m_index, attributes);
	if (res != dalia::Result::Ok) m_result = res;
}

void Listener::Draw3D(bool isSelected) {
	if (!m_isActive || (isPiloted && targetBody == TargetBody::Both)) return;

	if (!isPiloted || targetBody != TargetBody::Head) {
		DrawSphere(m_position, 0.5f, Fade(SKYBLUE, 0.8f));

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
	}

	if (!isPiloted || targetBody != TargetBody::Head)
	if (Vector3Distance(m_position, m_probePosition) > 0.01f) {
		DrawSphere(m_probePosition, 0.2f, ORANGE);
		DrawLine3D(m_position, m_probePosition, Fade(ORANGE, 0.5f));
	}

	if (isSelected) DrawSphereWires(m_position, 0.6f, 8, 8, WHITE);
}

void Listener::DrawInspectorUI(const UIContext& ui) {
	ImGui::PushID(this);

	ImGui::PushFont(ui.headerFont);
	ImGui::Text("[L] %u", m_index);
	ImGui::PopFont();
	ImGui::Separator();

	dalia::Result res;

	if (m_index != 0) {
		if (ImGui::Checkbox("Active", &m_isActive)) {
			res = m_engine->SetListenerActive(m_index, m_isActive);
			if (res != dalia::Result::Ok) m_result = res;
		}
	}
	else {
		ImGui::Text("Listener 0 is always active");
	}

	if (m_isActive) {
		ImGui::Text("Target Body:");
		ImGui::SameLine();

		int* target = reinterpret_cast<int*>(&targetBody);
		ImGui::RadioButton("Both", target, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Head", target, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Probe", target, 2);

		if (!isPiloted) {
			if (ImGui::Button("Pilot")) isPiloted = true;
		}
		else {
			if (ImGui::Button("Stop Piloting")) isPiloted = false;
		}

		if (!isPiloted) {
			// TODO: Add transform setters for velocity
		}
	}

	ImGui::PopID();
}

void Listener::SyncWithCamera(const Camera3D& camera) {
	if (targetBody == TargetBody::Both) {
		m_position = camera.position;
		m_probePosition = camera.position;
		m_forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
		m_up = camera.up;
	}
	else if (targetBody == TargetBody::Head) {
		m_position = camera.position;
		m_forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
		m_up = camera.up;
	}
	else if (targetBody == TargetBody::Probe) {
		m_probePosition = camera.position;
	}
}
