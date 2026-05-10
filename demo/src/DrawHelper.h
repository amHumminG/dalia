#pragma once

#include "raylib.h"
#include "imgui.h"

#include "Movement.h"

#include <cmath>

void DrawEditorGrid(int slices, float spacing, Color baseColor);

void DrawSelectionAnchor(Vector3 position, float radius, Color color);

void DrawMovementInspector(MovementState& state, Vector3 currentPosition, const Vector3& derivedVelocity);

