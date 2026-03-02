#pragma once
#include "core/SPSCRingBuffer.h"
#include <vector>

namespace dalia {

	struct RtCommand {
		enum class Type {
			None,
			Play,
			Pause,
			Stop,
			SetVolume,
			SwapMixOrder,
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
			struct { uint32_t* ptr; uint32_t nodeCount; } graph;
		} data = {};

		static RtCommand Play() {
			RtCommand cmd;
			return cmd;
		}

		static RtCommand Pause() {
			RtCommand cmd;
			return cmd;
		}

		static RtCommand Stop() {
			RtCommand cmd;
			return cmd;
		}

		static RtCommand SetVolume() {
			RtCommand cmd;
			return cmd;
		}

		 static RtCommand SwapMixOrder(uint32_t* ptr, uint32_t nodeCount) {
			RtCommand cmd;
			cmd.type = RtCommand::Type::SwapMixOrder;
			cmd.data.graph.ptr = ptr;
			cmd.data.graph.nodeCount = nodeCount;
			return cmd;
		}
	};

	class RtCommandQueue {
	public:
		RtCommandQueue(size_t commandCapacity);
		~RtCommandQueue() = default;

		// Game thread API
		void Enqueue(const RtCommand& command);
		void Dispatch();

		// Audio thread API
		bool Pop(RtCommand& command);

	private:
		std::vector<RtCommand> m_stagingArea;
		SPSCRingBuffer<RtCommand> m_buffer;
	};
}