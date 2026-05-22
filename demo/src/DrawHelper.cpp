#include "DrawHelper.h"

#include "rlgl.h"

#include <math.h>
#include <algorithm>

#include "raymath.h"

void DrawEditorGrid(int slices, float spacing, Color baseColor) {
	int halfSlices = slices / 2;
	float limit = static_cast<float>(halfSlices) * spacing;

	rlEnableColorBlend();
	rlBegin(RL_LINES);

	rlColor4ub(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
	for (int i = -halfSlices; i <= halfSlices; i++) {
		if (i == 0) continue;

		float pos = static_cast<float>(i) * spacing;

		// Z-axis lines
		rlVertex3f(pos, 0.0f, -limit);
		rlVertex3f(pos, 0.0f, limit);

		// X-axis lines
		rlVertex3f(-limit, 0.0f, pos);
		rlVertex3f(limit, 0.0f, pos);
	}

	// Axis Lines
	unsigned char alpha = 190;

	// X (Red)
	rlColor4ub(255, 70, 70, alpha);
	rlVertex3f(-limit, 0.0f, 0.0f);
	rlVertex3f(limit, 0.0f, 0.0f);

	// Y (Green)
	rlColor4ub(70, 255, 70, alpha);
	rlVertex3f(0.0f, -limit, 0.0f);
	rlVertex3f(0.0f, limit, 0.0f);

	// Z (Blue)
	rlColor4ub(70, 70, 255, alpha);
	rlVertex3f(0.0f, 0.0f, -limit);
	rlVertex3f(0.0f, 0.0f, limit);

	rlEnd();
}

void DrawSelectionAnchor(Vector3 position, float radius, Color color) {
	Vector3 groundPos = { position.x, 0.0f, position.z };
	DrawLine3D(position, groundPos, Fade(color, 0.5f));

	rlBegin(RL_LINES);
	rlColor4ub(color.r, color.g, color.b, color.a);

	int segments = 32;
	for (int i = 0; i < segments; i++) {
		float angle1 = static_cast<float>(i) / segments * 2.0f * PI;
		float angle2 = static_cast<float>(i + 1) / segments * 2.0f * PI;

		rlVertex3f(groundPos.x + cosf(angle1) * radius, 0.0f, groundPos.z + sinf(angle1) * radius);
		rlVertex3f(groundPos.x + cosf(angle2) * radius, 0.0f, groundPos.z + sinf(angle2) * radius);
	}

	rlEnd();
}

void DrawMovementInspector(MovementState& state, Vector3 currentPosition, const Vector3& derivedVelocity) {
	ImGui::PushID(&state);

	// Preset type
	if (ImGui::CollapsingHeader("Movement & Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
		// Preset selector
		const char* presets[] = {
			"Manual",
			"OrbitXZ",
			"OrbitXY",
			"OscillateX",
			"OscillateY",
			"OscillateZ",
			"FlybyX",
			"FlybyY",
			"FlybyZ"
		};
		int currentPreset = static_cast<int>(state.type);

		if (ImGui::Combo("Preset", &currentPreset, presets, IM_ARRAYSIZE(presets))) {
			state.type = static_cast<MovementPreset>(currentPreset);

			state.timeAccumulator = 0.0f;
			state.centerPoint = currentPosition;
		}

		// Parameters
		if (state.type != MovementPreset::Manual) {
			ImGui::Indent();

			ImGui::DragFloat("Speed", &state.speed, 0.1f, -50.0f, 50.0f, "%.2f");
			ImGui::DragFloat("Radius", &state.radius, 0.1f, 0.1f, 100.0f, "%.2f");

			ImGui::Unindent();
		}

		ImGui::Spacing();
		ImGui::Text("Center: [%.1f, %.1f, %.1f]", state.centerPoint.x, state.centerPoint.y, state.centerPoint.z);
		if (ImGui::Button("Set Center to Current Position", ImVec2(-1.0f, 0))) {
			state.centerPoint = currentPosition;
		}
	}

	ImGui::Separator();

	// Velocity Display
	ImGui::TextDisabled("Derived Velocity (m/s)");
	ImGui::Text("X: %6.2f | Y: %6.2f | Z: %6.2f", derivedVelocity.x, derivedVelocity.y, derivedVelocity.z);

	float derivedSpeed = sqrtf(
		(derivedVelocity.x * derivedVelocity.x) +
		(derivedVelocity.y * derivedVelocity.y) +
		(derivedVelocity.z * derivedVelocity.z)
	);
	ImGui::TextDisabled("Derived Speed: %.2f (m/s)", derivedSpeed);

	ImGui::PopID();
}
