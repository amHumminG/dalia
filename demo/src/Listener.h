#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"
#include "UI.h"

class Listener {
public:
	Listener(dalia::Engine* engine, uint32_t index, bool isActive=false);

	void Update();
	void Draw3D(bool isSelected);
	void DrawInspectorUI(const UIContext& ui);

	void SyncWithCamera(const Camera3D& camera);

	bool IsActive() const { return m_isActive; }

	uint32_t GetIndex() const { return m_index; }

	Vector3 GetPosition() const { return m_position; }
	void SetPosition(const Vector3& position) { m_position = position; }

	Vector3 GetProbePosition() const { return m_probePosition; }
	void SetProbePosition(const Vector3& probePosition) { m_probePosition = probePosition; }

	Vector3 GetForward() const { return m_forward; }
	void SetForward(const Vector3& forward) { m_forward = forward; }

	bool isPiloted = false;
	enum class TargetBody { Both, Head, Probe };
	TargetBody targetBody = TargetBody::Both;

private:
	dalia::Engine* m_engine = nullptr;
	dalia::Result m_result = dalia::Result::Ok;

	uint32_t m_index;
	bool m_isActive = false;

	Vector3 m_position = {0.0f, 0.0f, 0.0f};
	Vector3 m_probePosition = {0.0f, 0.0f, 0.0f};
	Vector3 m_velocity = {0.0f, 0.0f, 0.0f};

	Vector3 m_forward = {0.0f, 0.0f, 1.0f};
	Vector3 m_up = {0.0f, 1.0f, 0.0f};
};
