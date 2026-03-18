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
				uint32_t channels;
				uint32_t sampleRate;
			} prepStreaming;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGeneration;

				const float* pcmData;
				uint32_t frameCount;
				uint32_t channels;
				uint32_t sampleRate;
			} prepResident;

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

		static RtCommand PrepareVoiceStreaming(uint32_t index, uint32_t generation, uint32_t streamIndex,
			uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd;
			cmd.type = Type::PrepareVoiceStreaming;
			cmd.data.prepStreaming.voiceIndex = index;
			cmd.data.prepStreaming.voiceGeneration = generation;
			cmd.data.prepStreaming.streamIndex = streamIndex;

			cmd.data.prepStreaming.channels = channels;
			cmd.data.prepStreaming.sampleRate = sampleRate;
			return cmd;
		}

		static RtCommand PrepareVoiceResident(uint32_t index, uint32_t generation, const float* dataPtr,
			uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd;
			cmd.type = Type::PrepareVoiceResident;
			cmd.data.prepResident.voiceIndex = index;
			cmd.data.prepResident.voiceGeneration = generation;

			cmd.data.prepResident.pcmData = dataPtr;
			cmd.data.prepResident.frameCount = frameCount;
			cmd.data.prepResident.channels = channels;
			cmd.data.prepResident.sampleRate = sampleRate;
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