#pragma once
#include "SPSCRingBuffer.h"
#include <vector>

namespace dalia {

	struct AudioCommand {
		enum class Type {
			None,
			Play,
			Pause,
			Stop,
			SetVolume,
			// ...
		};
		Type type = Type::None;
		uint64_t uuid; // AudioHandle uuid
		union {
			uint32_t assetID; // Unique identifier for a sound in a soundbank (could be string?)
			float fvalue;
			bool bvalue;
			struct { float x, y, z; } vector3;
		} data;	// Command data (will likely be expanded later)
	};

	class CommandQueue {
	public:
		CommandQueue(size_t commandCapacity);
		~CommandQueue() = default;

		// Game thread API
		void Enqueue(const AudioCommand& command);
		void Dispatch();

		// Audio thread API
		bool Pop(AudioCommand& command);

	private:
		std::vector<AudioCommand> m_stagingArea;
		SPSCRingBuffer<AudioCommand> m_buffer;
	};
}