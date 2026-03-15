// Raylib / ImGui
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

#include "dalia.h"

#include <chrono>
#include <thread>
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
		// assets/Faouzia - UNETHICAL.ogg
		// assets/spiderman.ogg

		// StreamSoundHandle soundHandle;
		// engine.LoadStreamSound(soundHandle, "assets/spiderman.ogg");

		ResidentSoundHandle soundHandle;
		engine.LoadResidentSound(soundHandle, "assets/spiderman.ogg");

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		PlaybackHandle playbackHandle;
		engine.CreatePlayback(playbackHandle, soundHandle);

		engine.Play(playbackHandle);

		while (true) {
			engine.Update();
		}

		engine.Deinit();
	}

	return 0;
}