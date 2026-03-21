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
			AllocateVoiceStreaming,
			AllocateVoiceResident,
			PlayVoice,
			PauseVoice,
			StopVoice,

			// Voice Properties
			SetVoiceParent,
			SetVoiceVolume,
			SetVoicePitch,
			SetVoicePan,
			SetVoicePosition,
			SetVoiceVelocity,

			// Bus Lifecycle
			AllocateBus,
			DeallocateBus,

			// Bus Properties
			SetBusParent
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
			} voice;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGeneration;
				uint32_t parentBusIndex;
			} voiceParent;

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

				uint32_t streamIndex;
				uint32_t channels;
				uint32_t sampleRate;
			} prepStreaming;

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

			struct {
				uint32_t busIndex;
				uint32_t parentBusIndex;
			} bus;

		} data = {};

		static RtCommand SwapMixOrder(uint32_t* ptr, uint32_t nodeCount) {
			RtCommand cmd;
			cmd.type = RtCommand::Type::SwapMixOrder;
			cmd.data.mixOrder.ptr = ptr;
			cmd.data.mixOrder.nodeCount = nodeCount;
			return cmd;
		}

		static RtCommand PrepareVoiceResident(uint32_t index, uint32_t generation, const float* dataPtr,
			uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd;
			cmd.type = Type::AllocateVoiceResident;
			cmd.data.prepResident.voiceIndex = index;
			cmd.data.prepResident.voiceGeneration = generation;

			cmd.data.prepResident.pcmData = dataPtr;
			cmd.data.prepResident.frameCount = frameCount;
			cmd.data.prepResident.channels = channels;
			cmd.data.prepResident.sampleRate = sampleRate;
			return cmd;
		}

		static RtCommand PrepareVoiceStreaming(uint32_t index, uint32_t generation, uint32_t streamIndex,
			uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd;
			cmd.type = Type::AllocateVoiceStreaming;
			cmd.data.prepStreaming.voiceIndex = index;
			cmd.data.prepStreaming.voiceGeneration = generation;
			cmd.data.prepStreaming.streamIndex = streamIndex;

			cmd.data.prepStreaming.channels = channels;
			cmd.data.prepStreaming.sampleRate = sampleRate;
			return cmd;
		}

		static RtCommand PlayVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.type = Type::PlayVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGeneration = generation;
			return cmd;
		}

		static RtCommand PauseVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.type = Type::PauseVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGeneration = generation;
			return cmd;
		}

		static RtCommand StopVoice(uint32_t index, uint32_t generation) {
			RtCommand cmd;
			cmd.type = Type::StopVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGeneration = generation;
			return cmd;
		}

		static RtCommand SetVoiceParent(uint32_t index, uint32_t generation, uint32_t parentBusIndex) {
			RtCommand cmd;
			cmd.type = Type::SetVoiceParent;
			cmd.data.voiceParent.voiceIndex = index;
			cmd.data.voiceParent.voiceGeneration = generation;
			cmd.data.voiceParent.parentBusIndex = parentBusIndex;
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

		static RtCommand AllocateBus(uint32_t index, uint32_t parentIndex) {
			RtCommand cmd;
			cmd.type = Type::AllocateBus;
			cmd.data.bus.busIndex = index;
			cmd.data.bus.parentBusIndex = parentIndex;
			return cmd;
		}

		static RtCommand DeallocateBus(uint32_t index) {
			RtCommand cmd;
			cmd.type = Type::DeallocateBus;
			cmd.data.bus.busIndex = index;
			return cmd;
		}

		static RtCommand SetBusParent(uint32_t index, uint32_t parentIndex) {
			RtCommand cmd;
			cmd.type = Type::SetBusParent;
			cmd.data.bus.busIndex = index;
			cmd.data.bus.parentBusIndex = parentIndex;
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