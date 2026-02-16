// Raylib / ImGui
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

// Audio Engine
#include "AudioEngine.h"

#include <iostream>

int main() {
	using namespace placeholder_name;

		AudioEngine engine;

		Result result = engine.init();
		if (result == Result::Ok) std::cout << "Initialized Engine" << std::endl;
		else std::cout << GetErrorString(result) << std::endl;

		result = engine.deinit();
		if (result == Result::Ok) std::cout << "Deinitialized Engine" << std::endl;
		else std::cout << GetErrorString(result) << std::endl;

	return 0;
}