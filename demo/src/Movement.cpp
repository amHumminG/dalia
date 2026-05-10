#include "Movement.h"

#include <cmath>

void UpdateMovementPreset(MovementState& state, Vector3& position, float deltaTime) {
	if (state.type == MovementPreset::Manual) return;

	state.timeAccumulator += deltaTime * state.speed;

	switch (state.type) {
		case MovementPreset::OrbitXZ:
			position.x = state.centerPoint.x + cosf(state.timeAccumulator) * state.radius;
			position.z = state.centerPoint.z + sinf(state.timeAccumulator) * state.radius;
			break;
		case MovementPreset::OrbitXY:
			position.x = state.centerPoint.x + cosf(state.timeAccumulator) * state.radius;
			position.y = state.centerPoint.y + sinf(state.timeAccumulator) * state.radius;
			break;
		case MovementPreset::OscillateX:
			position.x = state.centerPoint.x + sinf(state.timeAccumulator) * state.radius;
			break;
		case MovementPreset::OscillateY:
			position.y = state.centerPoint.y + sinf(state.timeAccumulator) * state.radius;
			break;
		case MovementPreset::OscillateZ:
			position.z = state.centerPoint.z + sinf(state.timeAccumulator) * state.radius;
			break;
		case MovementPreset::FlybyX:
		case MovementPreset::FlybyY:
		case MovementPreset::FlybyZ:
			float travelDistance = (state.radius * 2.0f);
			float offset = fmodf(state.timeAccumulator, travelDistance) - state.radius;
			if (state.type == MovementPreset::FlybyX) position.x = state.centerPoint.x + offset;
			if (state.type == MovementPreset::FlybyY) position.y = state.centerPoint.y + offset;
			if (state.type == MovementPreset::FlybyZ) position.z = state.centerPoint.z + offset;
			break;
	}
}