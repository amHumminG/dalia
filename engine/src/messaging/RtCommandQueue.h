#pragma once
#include "core/SPSCRingBuffer.h"
#include <vector>

namespace dalia {

	struct RtCommand {
		enum class Type : uint8_t {
			None,

			// General
			SwapMixOrder,

			// Voice Lifecycle
			PrepareVoice,
			PlayVoice,
			PauseVoice,
			StopVoice,

			// Voice Parameters
			SetVoiceVolume,
			SetVoicePitch,
			SetVoicePan,
			SetVoicePosition,
			SetVoiceVelocity
		};

		Type type = Type::None;

		// Payload
		union {
			struct {
				uint32_t* ptr;
				uint32_t nodeCount;
			} mixOrder;

			struct {
				uint32_t index;
				uint32_t generation;
				uint32_t assetId; // Assuming this is how we store assets in loaded soundbanks
			} voicePrep;

			struct {
				uint32_t index;
				uint32_t generation;
			} voiceState;

			struct {
				uint32_t index;
				uint32_t generation;
				float value;
			} voiceFloat;

			struct {
				uint32_t index;
				uint32_t generation;
				float x, y, z;
			} voiceVector3;

		} data = {};

		static RtCommand SwapMixOrder(uint32_t* ptr, uint32_t nodeCount) {
			RtCommand cmd;
			cmd.type = RtCommand::Type::SwapMixOrder;
			cmd.data.mixOrder.ptr = ptr;
			cmd.data.mixOrder.nodeCount = nodeCount;
			return cmd;
		}

		static RtCommand PrepareVoice(uint32_t index, uint32_t generation, uint32_t assetId) {
			RtCommand cmd;
			cmd.type = Type::PrepareVoice;
			cmd.data.voicePrep.index = index;
			cmd.data.voicePrep.generation = generation;
			cmd.data.voicePrep.assetId = assetId;
			return cmd;
		}

		static RtCommand PlayVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.data.voiceState.index = index;
			cmd.data.voiceState.generation = generation;
			return cmd;
		}

		static RtCommand PauseVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.data.voiceState.index = index;
			cmd.data.voiceState.generation = generation;
			return cmd;
		}

		static RtCommand StopVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.data.voiceState.index = index;
			cmd.data.voiceState.generation = generation;
			return cmd;
		}

		static RtCommand SetVolume(uint32_t index, uint32_t generation, float value) {
			RtCommand cmd;
			cmd.data.voiceFloat.index = index;
			cmd.data.voiceFloat.generation = generation;
			cmd.data.voiceFloat.value = value;
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