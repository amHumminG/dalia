// Raylib / ImGui
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"
#include "imgui.h"

// Audio Engine
#include "AudioEngine.h"
#include "../src/CommandQueue.h"
#include "../src/EventQueue.h"

#include <iostream>

int main() {
	using namespace placeholder_name;

	// Testing AudioEngine
	{
		std::cout << "Testing Engine Init/Deinit" << std::endl;

		AudioEngine engine;
		EngineConfig config;
		config.logLevel = LogLevel::Debug;
		Result result = engine.Init(config);
		result = engine.Deinit();
	}

	// Testing Command/Event Queues (Temporary)
	{
		std::cout << "Starting Queue Test" << std::endl;
		size_t queueSize = 8;

		// COMMAND QUEUE
		CommandQueue cmdQueue(queueSize);

		AudioCommand cmdSend;
		cmdSend.type = AudioCommand::Type::Play;
		cmdSend.data.assetID = 15;
		cmdQueue.Enqueue(cmdSend);

		AudioCommand cmdRecieve;
		assert(cmdQueue.Pop(cmdRecieve) == false && "Pop should fail before flush");
		cmdQueue.Flush();
		assert(cmdQueue.Pop(cmdRecieve) == true && "Pop should succeed after flush");
		assert(cmdRecieve.type == AudioCommand::Type::Play);
		assert(cmdRecieve.data.assetID == 15);
		assert(cmdQueue.Pop(cmdRecieve) == false && "cmdQueue should now be empty");
	
		// EVENT QUEUE
		EventQueue eventQueue(queueSize);

		AudioEvent eventSend;
		eventSend.type = AudioEvent::Type::SoundFinished;
		assert(eventQueue.Push(eventSend) == true && "Event push should succeed");

		AudioEvent eventRecieve;
		assert(eventQueue.Pop(eventRecieve) == true && "Pop should find the event");
		assert(eventRecieve.type == AudioEvent::Type::SoundFinished);

		for (int i = 0; i < queueSize; ++i) eventQueue.Push(eventRecieve);
		assert(eventQueue.Push(eventSend) == false && "Queue should reject when full");

		std::cout << "All Queue tests passed successfully!" << std::endl;
	}

	return 0;
}