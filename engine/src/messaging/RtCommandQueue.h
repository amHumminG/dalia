#pragma once

#include "core/Constants.h"
#include "core/SPSCRingBuffer.h"
#include "dalia/audio/EffectControl.h"

#include <vector>

namespace dalia {

	struct RtCommand {
		enum class Type : uint8_t {
			None,

			// Listener
			SetListenerActive,
			SetListenerInactive,

			// Voice Lifecycle
			AllocateVoice,
			DeallocateVoice,
			PrepareVoiceStreaming,
			PrepareVoiceResident,
			SeekVoice,
			PlayVoice,
			PauseVoice,
			StopVoice,

			// Voice Properties
			SetVoiceParent,
			SetVoiceLooping,
			SetVoiceGainMatrix,
			SetVoicePitch,

			// Bus Lifecycle
			AllocateBus,
			DeallocateBus,

			// Bus Properties
			SetBusParent,
			SetBusGain,

			// Effects
			AllocateBiquad,
			SetBiquadParams,

			AttachEffect,
			FadeDetachEffect,
			ForceDetachEffect,
			DeallocateEffect
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
				float gainMatrix[CHANNELS_MAX][CHANNELS_MAX];
			} gain;

			struct {
				BiquadFilterType type;
				BiquadConfig config;
			} biquad;

			struct {
				EffectType type;
				uint32_t busIndex;
				uint32_t effectSlot;
			} effect;

		} data = {};

		static RtCommand SetListenerActive(uint32_t index) {
			RtCommand cmd{};
			cmd.type = Type::SetListenerActive;
			cmd.targetIndex = index;
			return cmd;
		}

		static RtCommand SetListenerInactive(uint32_t index) {
			RtCommand cmd{};
			cmd.type = Type::SetListenerInactive;
			cmd.targetIndex = index;
			return cmd;
		}

		static RtCommand AllocateVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::AllocateVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static RtCommand DeallocateVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::DeallocateVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static RtCommand PrepareVoiceResident(uint32_t index, uint32_t gen, const float* dataPtr,
			uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd{};
			cmd.type = Type::PrepareVoiceResident;
			cmd.targetIndex = index;
			cmd.targetGen = gen;

			cmd.data.prepResident.pcmData = dataPtr;
			cmd.data.prepResident.frameCount = frameCount;
			cmd.data.prepResident.channels = channels;
			cmd.data.prepResident.sampleRate = sampleRate;
			return cmd;
		}

		static RtCommand PrepareVoiceStreaming(uint32_t index, uint32_t gen, uint32_t streamIndex,
			uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd{};
			cmd.type = Type::PrepareVoiceStreaming;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.prepStreaming.streamIndex = streamIndex;

			cmd.data.prepStreaming.channels = channels;
			cmd.data.prepStreaming.sampleRate = sampleRate;
			return cmd;
		}

		static RtCommand SeekVoice(uint32_t index, uint32_t gen, uint32_t seekFrame) {
			RtCommand cmd{};
			cmd.type = Type::SeekVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.seek.seekFrame = seekFrame;
			return cmd;
		}

		static RtCommand PlayVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::PlayVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static RtCommand PauseVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::PauseVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static RtCommand StopVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::StopVoice;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			return cmd;
		}

		static RtCommand SetVoiceParent(uint32_t index, uint32_t gen, uint32_t parentBusIndex) {
			RtCommand cmd{};
			cmd.type = Type::SetVoiceParent;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.setParent.parentIndex = parentBusIndex;
			return cmd;
		}

		static RtCommand SetVoiceLooping(uint32_t index, uint32_t gen, bool looping) {
			RtCommand cmd{};
			cmd.type = Type::SetVoiceLooping;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.boolVal.value = looping;
			return cmd;
		}

		static RtCommand SetVoiceGainMatrix(uint32_t index, uint32_t gen,
			const float gainMatrix[CHANNELS_MAX][CHANNELS_MAX]) {
			RtCommand cmd{};
			cmd.type = Type::SetVoiceGainMatrix;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			std::memcpy(cmd.data.gain.gainMatrix, gainMatrix, sizeof(cmd.data.gain.gainMatrix));
			return cmd;
		}

		static RtCommand AllocateBus(uint32_t index, uint32_t parentIndex) {
			RtCommand cmd{};
			cmd.type = Type::AllocateBus;
			cmd.targetIndex = index;
			cmd.data.setParent.parentIndex = parentIndex;
			return cmd;
		}

		static RtCommand DeallocateBus(uint32_t index) {
			RtCommand cmd{};
			cmd.type = Type::DeallocateBus;
			cmd.targetIndex = index;
			return cmd;
		}

		static RtCommand SetBusParent(uint32_t index, uint32_t parentIndex) {
			RtCommand cmd{};
			cmd.type = Type::SetBusParent;
			cmd.targetIndex = index;
			cmd.data.setParent.parentIndex = parentIndex;
			return cmd;
		}

		static RtCommand SetBusGain(uint32_t index, float gain) {
			RtCommand cmd{};
			cmd.type = Type::SetBusGain;
			cmd.targetIndex = index;
			cmd.data.floatVal.value = gain;
			return cmd;
		}

		static RtCommand AllocateBiquad(uint32_t index, uint32_t gen, BiquadFilterType type, const BiquadConfig& config) {
			RtCommand cmd{};
			cmd.type = Type::AllocateBiquad;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.biquad.type = type;
			cmd.data.biquad.config = config;
			return cmd;
		}

		static RtCommand SetBiquadParams(uint32_t index, uint32_t gen, const BiquadConfig& config) {
			RtCommand cmd{};
			cmd.type = Type::SetBiquadParams;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.biquad.config = config;
			return cmd;
		}

		static_assert(std::is_trivially_copyable_v<BiquadConfig>,
			"BiquadConfig must remain trivially copyable for use in RtCommands");

		static RtCommand AttachEffect(uint32_t index, uint32_t gen, EffectType type, uint32_t busIndex,
			uint32_t effectSlot) {
			RtCommand cmd{};
			cmd.type = Type::AttachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static RtCommand FadeDetachEffect(uint32_t index, uint32_t gen, EffectType type, uint32_t busIndex,
			uint32_t effectSlot) {
			RtCommand cmd{};
			cmd.type = Type::FadeDetachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static RtCommand ForceDetachEffect(uint32_t index, uint32_t gen, EffectType type, uint32_t busIndex,
			uint32_t effectSlot) {
			RtCommand cmd{};
			cmd.type = Type::ForceDetachEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static RtCommand DeallocateEffect(uint32_t index, uint32_t gen, EffectType type) {
			RtCommand cmd{};
			cmd.type = Type::DeallocateEffect;
			cmd.targetIndex = index;
			cmd.targetGen = gen;
			cmd.data.effect.type = type;
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