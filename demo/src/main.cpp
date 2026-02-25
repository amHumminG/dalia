// Raylib / ImGui
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

// Audio Engine
#include "dalia/AudioEngine.h"

#include <iostream>

int main() {
	using namespace dalia;

	// Testing AudioEngine
	{
		std::cout << "Testing Engine Init/Deinit" << std::endl;

		AudioEngine engine;
		EngineConfig config;
		config.logLevel = LogLevel::Debug;
		Result result = engine.Init(config);
		result = engine.Deinit();
	}

	return 0;
}