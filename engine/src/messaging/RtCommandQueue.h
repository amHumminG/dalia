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
			PrepareVoiceStreaming,
			PrepareVoiceResident,
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
				uint32_t voiceIndex;
				uint32_t voiceGeneration;
				uint32_t streamIndex;
			} voicePrepStreaming;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGeneration;
				uint32_t assetId; // Assuming this is how we store assets in loaded soundbanks
			} voicePrepResident;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGeneration;
			} voiceState;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGeneration;
				float value;
			} voiceFloat;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGeneration;
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

		static RtCommand PrepareVoiceStreaming(uint32_t index, uint32_t generation, uint32_t streamIndex) {
			RtCommand cmd;
			cmd.type = Type::PrepareVoiceStreaming;
			cmd.data.voicePrepStreaming.voiceIndex = index;
			cmd.data.voicePrepStreaming.voiceGeneration = generation;
			cmd.data.voicePrepStreaming.streamIndex = streamIndex;
			return cmd;
		}

		static RtCommand PrepareVoiceResident(uint32_t index, uint32_t generation, uint32_t assetId) {
			RtCommand cmd;
			cmd.type = Type::PrepareVoiceResident;
			cmd.data.voicePrepResident.voiceIndex = index;
			cmd.data.voicePrepResident.voiceGeneration = generation;
			cmd.data.voicePrepResident.assetId = assetId;
			return cmd;
		}

		static RtCommand PlayVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.type = Type::PlayVoice;
			cmd.data.voiceState.voiceIndex = index;
			cmd.data.voiceState.voiceGeneration = generation;
			return cmd;
		}

		static RtCommand PauseVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.type = Type::PauseVoice;
			cmd.data.voiceState.voiceIndex = index;
			cmd.data.voiceState.voiceGeneration = generation;
			return cmd;
		}

		static RtCommand StopVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.type = Type::StopVoice;
			cmd.data.voiceState.voiceIndex = index;
			cmd.data.voiceState.voiceGeneration = generation;
			return cmd;
		}

		static RtCommand SetVolume(uint32_t index, uint32_t generation, float value) {
			RtCommand cmd;
			cmd.type = Type::SetVoiceVolume;
			cmd.data.voiceFloat.voiceIndex = index;
			cmd.data.voiceFloat.voiceGeneration = generation;
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