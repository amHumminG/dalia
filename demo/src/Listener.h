#pragma once

#include "raylib.h"
#include "dalia.h"

class Listener {
public:
	Listener(uint32_t index, bool isActive=false);

	void Update(dalia::Engine& engine);
	void Draw3D();
	void DrawInspectorUI(dalia::Engine& engine);

	Vector3 GetPosition() const { return m_position; }

private:
	uint32_t m_index;
	bool m_isActive = false;

	Vector3 m_position = {0.0f, 0.0f, 0.0f};
	Vector3 m_probePosition = {0.0f, 0.0f, 0.0f};
	Vector3 m_velocity = {0.0f, 0.0f, 0.0f};

	Vector3 m_forward = {0.0f, 0.0f, 0.0f};
	Vector3 m_up = {0.0f, 0.0f, 0.0f};
};
