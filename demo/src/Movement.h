#pragma once

#include "raylib.h"

enum class MovementPreset {
	Manual = 0,
	OrbitXZ,
	OrbitXY,
	OscillateX,
	OscillateY,
	OscillateZ,
	FlybyX,
	FlybyY,
	FlybyZ
};

struct MovementState {
	MovementPreset type = MovementPreset::Manual;
	float speed = 5.0f;
	float radius = 5.0f;
	float timeAccumulator = 0.0f;
	Vector3 centerPoint = { 0.0f, 0.0f, 0.0f };
};

void UpdateMovementPreset(MovementState& state, Vector3& position, float deltaTime);