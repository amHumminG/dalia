// Raylib / ImGui
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"

#include <iostream>

int main() {
	using namespace dalia;

	// Testing Engine
	{
		std::cout << "Testing Engine Init/Deinit" << std::endl;

		Engine engine;
		EngineConfig config;
		config.logLevel = LogLevel::Debug;
		Result result = engine.Init(config);
		result = engine.Deinit();
	}

	return 0;
}