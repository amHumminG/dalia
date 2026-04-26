#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"

class Listener {
public:
	Listener(dalia::Engine* engine, uint32_t index, bool isActive=false);

	void Update();
	void Draw3D(bool isSelected);
	void DrawInspectorUI();

	bool isPiloted = false;
	void SyncWithCamera(const Camera3D& camera);

	uint32_t GetIndex() const { return m_index; }
	Vector3 GetPosition() const { return m_position; }

private:
	dalia::Engine* m_engine = nullptr;
	dalia::Result m_result = dalia::Result::Ok;

	enum class PilotTarget { Head, Probe };
	PilotTarget m_pilotTarget = PilotTarget::Head;

	uint32_t m_index;
	bool m_isActive = false;

	Vector3 m_position = {0.0f, 0.0f, 0.0f};
	Vector3 m_ProbePosition = {0.0f, 0.0f, 0.0f};
	Vector3 m_velocity = {0.0f, 0.0f, 0.0f};

	Vector3 m_forward = {0.0f, 0.0f, 1.0f};
	Vector3 m_up = {0.0f, 1.0f, 0.0f};
};
