#pragma once
#include "core/SPSCRingBuffer.h"
#include <vector>

namespace dalia {

	struct AudioCommand {
		enum class Type {
			None,
			Play,
			Pause,
			Stop,
			SetVolume,
			SwapGraph,
			// ...
		};
		Type type = Type::None;
		uint32_t voicePoolIndex;
		uint32_t generation;
		union {
			uint32_t assetID; // Unique identifier for a sound in a soundbank (could be string?)
			float fvalue;
			bool bvalue;
			struct { float x, y, z; } vector3;
			struct { bool useListA; uint32_t nodeCount; } graph;
		} data = {};

		static AudioCommand Play() {
			AudioCommand cmd;
			return cmd;
		}

		static AudioCommand Pause() {
			AudioCommand cmd;
			return cmd;
		}

		static AudioCommand Stop() {
			AudioCommand cmd;
			return cmd;
		}

		static AudioCommand SetVolume() {
			AudioCommand cmd;
			return cmd;
		}

		 static AudioCommand SwapGraph(bool useListA, uint32_t nodeCount) {
			AudioCommand cmd;
			cmd.type = AudioCommand::Type::SwapGraph;
			cmd.data.graph.useListA = useListA;
			cmd.data.graph.nodeCount = nodeCount;
			return cmd;
		}
	};

	class AudioCommandQueue {
	public:
		AudioCommandQueue(size_t commandCapacity);
		~AudioCommandQueue() = default;

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