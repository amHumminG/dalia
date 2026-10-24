#pragma once

#include "core/SPSCRingBuffer.h"
#include "dalia/EffectControl.h"
#include "dalia/PlaybackControl.h"

#include <vector>

namespace dalia {

	struct AsyncMixerCommand {
		enum class Type : uint8_t {
			None,

			// Voice
			AllocateVoice,
			FreeVoice,
			PrepareVoiceStreaming,
			PrepareVoiceResident,
			SeekVoice,
			PlayVoice,
			PauseVoice,
			StopVoice,
			SetVoiceParent,

			// Bus
			AllocateBus,
			FreeBus,
			SetBusParent,

			// Effects
			AllocateEffect,
			FreeEffect,
			AttachEffect,
			FadeDetachEffect,
			ForceDetachEffect,

			// Misc
			SetGlobalDopplerFactor,


			Count // Determines the number of types
		};

		Type type = Type::None;

		uint32_t targetIndex;
		uint32_t targetGen;

		// Payload
		union {
			struct {
				uint32_t* ptr;
				uint32_t nodeCount;
			} mixOrder;

			struct {
				uint32_t parentIndex;
			} setParent;

			struct {
				const float* pcmData;
				uint32_t totalFrames;
				uint32_t channels;
				uint32_t sampleRate;
			} prepResident;

			struct {
				uint32_t streamIndex;
				uint32_t streamGen;
				uint32_t channels;
				uint32_t sampleRate;
			} prepStreaming;

			struct {
				uint32_t seekFrame;
			} seek;

			struct {
				bool value;
			} boolVal;

			struct {
				float value;
			} floatVal;

			struct {
				EffectType type;
				uint32_t busIndex;
				uint32_t effectSlot;
			} effect;

		} data = {};

		static AsyncMixerCommand AllocateVoice(uint32_t index, uint32_t gen) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::AllocateVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static AsyncMixerCommand FreeVoice(uint32_t index, uint32_t gen) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::FreeVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static AsyncMixerCommand PrepareVoiceResident(uint32_t index, uint32_t gen, const float* dataPtr,
			uint32_t totalFrames, uint32_t channels, uint32_t sampleRate) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::PrepareVoiceResident;
			cmd.targetIndex = index;
			cmd.targetGen = gen;

			cmd.data.prepResident.pcmData = dataPtr;
			cmd.data.prepResident.totalFrames = totalFrames;
			cmd.data.prepResident.channels = channels;
			cmd.data.prepResident.sampleRate = sampleRate;
			return cmd;
		}

		static AsyncMixerCommand PrepareVoiceStreaming(uint32_t index, uint32_t gen, uint32_t streamIndex, uint32_t streamGen,
			uint32_t channels, uint32_t sampleRate) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::PrepareVoiceStreaming;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.prepStreaming.streamIndex = streamIndex;
			cmd.data.prepStreaming.streamGen = streamGen;

			cmd.data.prepStreaming.channels = channels;
			cmd.data.prepStreaming.sampleRate = sampleRate;
			return cmd;
		}

		static AsyncMixerCommand SeekVoice(uint32_t index, uint32_t gen, uint32_t seekFrame) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::SeekVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.seek.seekFrame = seekFrame;
			return cmd;
		}

		static AsyncMixerCommand PlayVoice(uint32_t index, uint32_t gen) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::PlayVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static AsyncMixerCommand PauseVoice(uint32_t index, uint32_t gen) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::PauseVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static AsyncMixerCommand StopVoice(uint32_t index, uint32_t gen) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::StopVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static AsyncMixerCommand SetVoiceParent(uint32_t index, uint32_t gen, uint32_t parentBusIndex) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::SetVoiceParent;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.setParent.parentIndex = parentBusIndex;
			return cmd;
		}

		static AsyncMixerCommand AllocateBus(uint32_t index, uint32_t parentIndex) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::AllocateBus;
			cmd.targetIndex = index;
			cmd.data.setParent.parentIndex = parentIndex;
			return cmd;
		}

		static AsyncMixerCommand FreeBus(uint32_t index) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::FreeBus;
			cmd.targetIndex = index;
			return cmd;
		}

		static AsyncMixerCommand SetBusParent(uint32_t index, uint32_t parentIndex) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::SetBusParent;
			cmd.targetIndex = index;
			cmd.data.setParent.parentIndex = parentIndex;
			return cmd;
		}

		static AsyncMixerCommand AllocateEffect(EffectType type, uint32_t index, uint32_t gen) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::AllocateEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			return cmd;
		}

		static AsyncMixerCommand FreeEffect(uint32_t index, uint32_t gen, EffectType type) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::FreeEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			return cmd;
		}

		static AsyncMixerCommand AttachEffect(uint32_t index, uint32_t gen, EffectType type,
			uint32_t busIndex, uint32_t effectSlot) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::AttachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static AsyncMixerCommand FadeDetachEffect(uint32_t index, uint32_t gen, EffectType type,
			uint32_t busIndex, uint32_t effectSlot) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::FadeDetachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static AsyncMixerCommand ForceDetachEffect(uint32_t index, uint32_t gen, EffectType type,
			uint32_t busIndex, uint32_t effectSlot) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::ForceDetachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static AsyncMixerCommand SetGlobalDopplerFactor(float value) {
			AsyncMixerCommand cmd{};
			cmd.type = Type::SetGlobalDopplerFactor;
			cmd.data.floatVal.value = value;
			return cmd;
		}
	};

	struct  AsyncMixerEvent {
		enum class Type {
			None,

			// Voice Lifecycle
			VoiceStopped,


			Count // Determines the number of types
		};

		Type type = Type::None;

		// Payload
		union Data {
			struct {
				uint32_t index;
				uint32_t gen;
				PlaybackExitCondition exitCondition;
			} voice;

			struct {
				uint64_t handleRawId;
			} effect;

		} data = {};

		static AsyncMixerEvent VoiceStopped(uint32_t index, uint32_t generation, PlaybackExitCondition exitCondition) {
			AsyncMixerEvent ev;
			ev.type = Type::VoiceStopped;
			ev.data.voice.index = index;
			ev.data.voice.gen = generation;
			ev.data.voice.exitCondition = exitCondition;
			return ev;
		}
	};

	// --- Queues ---

	// Wrapper due to the need for a staging area
	class AsyncMixerCommandQueue {
	public:
		AsyncMixerCommandQueue(size_t commandCapacity);
		~AsyncMixerCommandQueue() = default;

		void Enqueue(const AsyncMixerCommand& command);
		void Dispatch();

		bool Pop(AsyncMixerCommand& command);

	private:
		std::vector<AsyncMixerCommand> m_stagingArea;
		SPSCRingBuffer<AsyncMixerCommand> m_buffer;
	};

	using AsyncMixerEventQueue = SPSCRingBuffer<AsyncMixerEvent>;
}