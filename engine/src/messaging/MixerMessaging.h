#pragma once

#include "core/SPSCRingBuffer.h"
#include "dalia/EffectControl.h"
#include "dalia/PlaybackControl.h"

#include <vector>

namespace dalia {

	struct MixerCommand {
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
			SetGlobalDopplerFactor
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
				uint32_t frameCount;
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

		static MixerCommand AllocateVoice(uint32_t index, uint32_t gen) {
			MixerCommand cmd{};
			cmd.type = Type::AllocateVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static MixerCommand FreeVoice(uint32_t index, uint32_t gen) {
			MixerCommand cmd{};
			cmd.type = Type::FreeVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static MixerCommand PrepareVoiceResident(uint32_t index, uint32_t gen, const float* dataPtr,
			uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
			MixerCommand cmd{};
			cmd.type = Type::PrepareVoiceResident;
			cmd.targetIndex = index;
			cmd.targetGen = gen;

			cmd.data.prepResident.pcmData = dataPtr;
			cmd.data.prepResident.frameCount = frameCount;
			cmd.data.prepResident.channels = channels;
			cmd.data.prepResident.sampleRate = sampleRate;
			return cmd;
		}

		static MixerCommand PrepareVoiceStreaming(uint32_t index, uint32_t gen, uint32_t streamIndex, uint32_t streamGen,
			uint32_t channels, uint32_t sampleRate) {
			MixerCommand cmd{};
			cmd.type = Type::PrepareVoiceStreaming;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.prepStreaming.streamIndex = streamIndex;
			cmd.data.prepStreaming.streamGen = streamGen;

			cmd.data.prepStreaming.channels = channels;
			cmd.data.prepStreaming.sampleRate = sampleRate;
			return cmd;
		}

		static MixerCommand SeekVoice(uint32_t index, uint32_t gen, uint32_t seekFrame) {
			MixerCommand cmd{};
			cmd.type = Type::SeekVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.seek.seekFrame = seekFrame;
			return cmd;
		}

		static MixerCommand PlayVoice(uint32_t index, uint32_t gen) {
			MixerCommand cmd{};
			cmd.type = Type::PlayVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static MixerCommand PauseVoice(uint32_t index, uint32_t gen) {
			MixerCommand cmd{};
			cmd.type = Type::PauseVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static MixerCommand StopVoice(uint32_t index, uint32_t gen) {
			MixerCommand cmd{};
			cmd.type = Type::StopVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static MixerCommand SetVoiceParent(uint32_t index, uint32_t gen, uint32_t parentBusIndex) {
			MixerCommand cmd{};
			cmd.type = Type::SetVoiceParent;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.setParent.parentIndex = parentBusIndex;
			return cmd;
		}

		static MixerCommand AllocateBus(uint32_t index, uint32_t parentIndex) {
			MixerCommand cmd{};
			cmd.type = Type::AllocateBus;
			cmd.targetIndex = index;
			cmd.data.setParent.parentIndex = parentIndex;
			return cmd;
		}

		static MixerCommand FreeBus(uint32_t index) {
			MixerCommand cmd{};
			cmd.type = Type::FreeBus;
			cmd.targetIndex = index;
			return cmd;
		}

		static MixerCommand SetBusParent(uint32_t index, uint32_t parentIndex) {
			MixerCommand cmd{};
			cmd.type = Type::SetBusParent;
			cmd.targetIndex = index;
			cmd.data.setParent.parentIndex = parentIndex;
			return cmd;
		}

		static MixerCommand AllocateEffect(EffectType type, uint32_t index, uint32_t gen) {
			MixerCommand cmd{};
			cmd.type = Type::AllocateEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			return cmd;
		}

		static MixerCommand FreeEffect(uint32_t index, uint32_t gen, EffectType type) {
			MixerCommand cmd{};
			cmd.type = Type::FreeEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			return cmd;
		}

		static MixerCommand AttachEffect(uint32_t index, uint32_t gen, EffectType type,
			uint32_t busIndex, uint32_t effectSlot) {
			MixerCommand cmd{};
			cmd.type = Type::AttachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static MixerCommand FadeDetachEffect(uint32_t index, uint32_t gen, EffectType type,
			uint32_t busIndex, uint32_t effectSlot) {
			MixerCommand cmd{};
			cmd.type = Type::FadeDetachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static MixerCommand ForceDetachEffect(uint32_t index, uint32_t gen, EffectType type,
			uint32_t busIndex, uint32_t effectSlot) {
			MixerCommand cmd{};
			cmd.type = Type::ForceDetachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static MixerCommand SetGlobalDopplerFactor(float value) {
			MixerCommand cmd{};
			cmd.type = Type::SetGlobalDopplerFactor;
			cmd.data.floatVal.value = value;
			return cmd;
		}
	};

	struct  MixerEvent {
		enum class Type {
			None,

			// Voice Lifecycle
			VoiceStopped,

			// Effects
			EffectActive,
			EffectDetached,
		};

		Type type = Type::None;

		// Payload
		union Data {
			struct {
				uint32_t index;
				uint32_t generation;
				PlaybackExitCondition exitCondition;
			} voice;

			struct {
				uint64_t handleRawId;
			} effect;

		} data = {};

		static MixerEvent VoiceStopped(uint32_t index, uint32_t generation, PlaybackExitCondition exitCondition) {
			MixerEvent ev;
			ev.type = Type::VoiceStopped;
			ev.data.voice.index = index;
			ev.data.voice.generation = generation;
			ev.data.voice.exitCondition = exitCondition;
			return ev;
		}

		static MixerEvent EffectActive(uint64_t handleRawId) {
			MixerEvent ev;
			ev.type = Type::EffectActive;
			ev.data.effect.handleRawId = handleRawId;
			return ev;
		}

		static MixerEvent EffectDetached(uint64_t handleRawId) {
			MixerEvent ev;
			ev.type = Type::EffectDetached;
			ev.data.effect.handleRawId = handleRawId;
			return ev;
		}
	};

	// --- Queues ---

	// Wrapper due to the need for a staging area
	class MixerCommandQueue {
	public:
		MixerCommandQueue(size_t commandCapacity);
		~MixerCommandQueue() = default;

		void Enqueue(const MixerCommand& command);
		void Dispatch();

		bool Pop(MixerCommand& command);

	private:
		std::vector<MixerCommand> m_stagingArea;
		SPSCRingBuffer<MixerCommand> m_buffer;
	};

	using MixerEventQueue = SPSCRingBuffer<MixerEvent>;
}