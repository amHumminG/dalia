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
		engine.Init(config);

		PlaybackHandle handle;
		// engine.CreateStreamPlayback(handle, "assets/Faouzia - UNETHICAL.ogg");
		engine.CreateStreamPlayback(handle, "assets/spiderman.ogg");
		engine.Play(handle);

		while (true) {
			engine.Update();
		}

		engine.Deinit();
	}

	return 0;
}