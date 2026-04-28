#include "Listener.h"

#include "rlgl.h"
#include "DrawHelper.h"

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
		float headRadius = 0.4f;
		DrawSphere(m_position, headRadius, Fade(SKYBLUE, 0.8f));

		Vector3 left = Vector3Normalize(Vector3CrossProduct(m_forward, m_up));
		Vector3 right = Vector3Normalize(Vector3CrossProduct(m_up, m_forward));

		Vector3 earLeft = {
			m_position.x + (left.x * headRadius),
			m_position.y + (left.y * headRadius),
			m_position.z + (left.z * headRadius)
		};
		Vector3 earRight = {
			m_position.x + (right.x * headRadius),
			m_position.y + (right.y * headRadius),
			m_position.z + (right.z * headRadius)
		};
		DrawSphere(earLeft, 0.15f, BLACK);
		DrawSphere(earRight, 0.15f, BLACK);

		// Headband
		int segments = 16;
		float bandRadius = headRadius + 0.05f;
		float bandThickness = 0.03f;

		Vector3 prevPoint = {
			m_position.x + (right.x * bandRadius),
			m_position.y + (right.y * bandRadius),
			m_position.z + (right.z * bandRadius)
		};

		for (int i = 1; i <= segments; i++) {
			float angle = (static_cast<float>(i) / segments) * PI;

			Vector3 nextPoint = {
				m_position.x + ((right.x * cosf(angle)) + (m_up.x * sinf(angle))) * bandRadius,
				m_position.y + ((right.y * cosf(angle)) + (m_up.y * sinf(angle))) * bandRadius,
				m_position.z + ((right.z * cosf(angle)) + (m_up.z * sinf(angle))) * bandRadius,
			};

			DrawCylinderEx(prevPoint, nextPoint, bandThickness, bandThickness, 6, BLACK);
			DrawSphere(nextPoint, bandThickness, BLACK);

			prevPoint = nextPoint;
		}

		// --- FACE ICONOGRAPHY ---
		float eyeWhiteRadius = 0.07f;
		float pupilRadius = 0.035f;
		float noseRadius = 0.08f;

		// Calculate normalized directions for the eyes
		Vector3 leftEyeDir = Vector3Normalize({
			m_forward.x + (m_up.x * 0.35f) + (left.x * 0.4f),
			m_forward.y + (m_up.y * 0.35f) + (left.y * 0.4f),
			m_forward.z + (m_up.z * 0.35f) + (left.z * 0.4f)
		});

		Vector3 rightEyeDir = Vector3Normalize({
			m_forward.x + (m_up.x * 0.35f) + (right.x * 0.4f),
			m_forward.y + (m_up.y * 0.35f) + (right.y * 0.4f),
			m_forward.z + (m_up.z * 0.35f) + (right.z * 0.4f)
		});

		// Position the Eye Whites on the surface
		Vector3 leftEye = Vector3Add(m_position, Vector3Scale(leftEyeDir, headRadius));
		Vector3 rightEye = Vector3Add(m_position, Vector3Scale(rightEyeDir, headRadius));

		DrawSphere(leftEye, eyeWhiteRadius, WHITE);
		DrawSphere(rightEye, eyeWhiteRadius, WHITE);

		// Position the Pupils
		float pupilOffset = headRadius + (eyeWhiteRadius * 0.7f);
		Vector3 leftPupil = Vector3Add(m_position, Vector3Scale(leftEyeDir, pupilOffset));
		Vector3 rightPupil = Vector3Add(m_position, Vector3Scale(rightEyeDir, pupilOffset));

		DrawSphere(leftPupil, pupilRadius, BLACK);
		DrawSphere(rightPupil, pupilRadius, BLACK);

		// Position the Nose
		Vector3 noseDir = Vector3Normalize({
			m_forward.x - (m_up.x * 0.1f),
			m_forward.y - (m_up.y * 0.1f),
			m_forward.z - (m_up.z * 0.1f)
		});

		Vector3 nosePos = Vector3Add(m_position, Vector3Scale(noseDir, headRadius));
		DrawSphere(nosePos, noseRadius, RED);
	}

	if (Vector3Distance(m_position, m_probePosition) > 0.01f) {
		DrawSphere(m_probePosition, 0.2f, ORANGE);
		DrawCylinderEx(m_position, m_probePosition, 0.015f, 0.015f, 8, Fade(ORANGE, 0.5f));
	}

	if (isSelected) {
		DrawSelectionAnchor(m_position, 0.6f, WHITE);
		if (Vector3Distance(m_probePosition, m_position) > 0.001f) DrawSelectionAnchor(m_probePosition, 0.3f, WHITE);
	}
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

	if (m_isActive) {
		ImGui::Text("Target Body:");
		ImGui::SameLine();

		int* target = reinterpret_cast<int*>(&targetBody);
		ImGui::RadioButton("Both", target, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Head", target, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Probe", target, 2);

		ImGui::SameLine();
		if (ImGui::Button("Reset Probe")) {
			m_probePosition = m_position;
		}

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
