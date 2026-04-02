#pragma once

#include "core/Constants.h"
#include "core/SPSCRingBuffer.h"
#include "dalia/audio/EffectControl.h"

#include <vector>

namespace dalia {

	struct RtCommand {
		enum class Type : uint8_t {
			None,

			// General
			SwapMixOrder,

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
			SetBusVolume,

			// Effects
			AllocateBiquad,
			SetBiquadParams,

			AttachEffect,
			FadeDetachEffect,
			ForceDetachEffect,
			DeallocateEffect
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
				uint32_t voiceGen;
			} voice;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;
				uint32_t parentBusIndex;
			} voiceParent;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;

				const float* pcmData;
				uint32_t frameCount;
				uint32_t channels;
				uint32_t sampleRate;
			} prepResident;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;

				uint32_t streamIndex;
				uint32_t channels;
				uint32_t sampleRate;
			} prepStreaming;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;
				uint32_t seekFrame;
			} voiceSeek;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;
				bool value;
			} voiceBool;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;
				float value;
			} voiceFloat;

			struct {
				uint32_t voiceIndex;
				uint32_t voiceGen;
				float gainMatrix[CHANNELS_MAX][CHANNELS_MAX];
			} voiceGain;

			struct {
				uint32_t busIndex;
				uint32_t parentBusIndex;
			} bus;

			struct {
				uint32_t busIndex;
				float value;
			} busFloat;

			struct {
				uint32_t index;
				uint32_t gen;
				BiquadFilterType type;
				BiquadConfig config;
			} biquad;

			struct {
				uint32_t index;
				uint32_t gen;
				EffectType type;
				uint32_t busIndex;
				uint32_t effectSlot;
			} effect;

		} data = {};

		static RtCommand SwapMixOrder(uint32_t* ptr, uint32_t nodeCount) {
			RtCommand cmd{};
			cmd.type = Type::SwapMixOrder;
			cmd.data.mixOrder.ptr = ptr;
			cmd.data.mixOrder.nodeCount = nodeCount;
			return cmd;
		}

		static RtCommand AllocateVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd;
			cmd.type = Type::AllocateVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGen = gen;
			return cmd;
		}

		static RtCommand DeallocateVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd;
			cmd.type = Type::DeallocateVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGen = gen;
			return cmd;
		}

		static RtCommand PrepareVoiceResident(uint32_t index, uint32_t gen, const float* dataPtr,
			uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
			RtCommand cmd{};
			cmd.type = Type::PrepareVoiceResident;
			cmd.data.prepResident.voiceIndex = index;
			cmd.data.prepResident.voiceGen = gen;

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
			cmd.data.prepStreaming.voiceIndex = index;
			cmd.data.prepStreaming.voiceGen = gen;
			cmd.data.prepStreaming.streamIndex = streamIndex;

			cmd.data.prepStreaming.channels = channels;
			cmd.data.prepStreaming.sampleRate = sampleRate;
			return cmd;
		}

		static RtCommand SeekVoice(uint32_t index, uint32_t gen, uint32_t seekFrame) {
			RtCommand cmd{};
			cmd.type = Type::SeekVoice;
			cmd.data.voiceSeek.voiceIndex = index;
			cmd.data.voiceSeek.voiceGen = gen;
			cmd.data.voiceSeek.seekFrame = seekFrame;
			return cmd;
		}

		static RtCommand PlayVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::PlayVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGen = gen;
			return cmd;
		}

		static RtCommand PauseVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::PauseVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGen = gen;
			return cmd;
		}

		static RtCommand StopVoice(uint32_t index, uint32_t gen) {
			RtCommand cmd{};
			cmd.type = Type::StopVoice;
			cmd.data.voice.voiceIndex = index;
			cmd.data.voice.voiceGen = gen;
			return cmd;
		}

		static RtCommand SetVoiceParent(uint32_t index, uint32_t gen, uint32_t parentBusIndex) {
			RtCommand cmd{};
			cmd.type = Type::SetVoiceParent;
			cmd.data.voiceParent.voiceIndex = index;
			cmd.data.voiceParent.voiceGen = gen;
			cmd.data.voiceParent.parentBusIndex = parentBusIndex;
			return cmd;
		}

		static RtCommand SetVoiceLooping(uint32_t index, uint32_t gen, bool looping) {
			RtCommand cmd{};
			cmd.type = Type::SetVoiceLooping;
			cmd.data.voiceBool.voiceIndex = index;
			cmd.data.voiceBool.voiceGen = gen;
			cmd.data.voiceBool.value = looping;
			return cmd;
		}

		static RtCommand SetVoiceGainMatrix(uint32_t index, uint32_t gen,
			const float gainMatrix[CHANNELS_MAX][CHANNELS_MAX]) {
			RtCommand cmd{};
			cmd.type = Type::SetVoiceGainMatrix;
			cmd.data.voiceGain.voiceIndex = index;
			cmd.data.voiceGain.voiceGen = gen;
			std::memcpy(cmd.data.voiceGain.gainMatrix, gainMatrix, sizeof(cmd.data.voiceGain.gainMatrix));
			return cmd;
		}

		static RtCommand AllocateBus(uint32_t index, uint32_t parentIndex) {
			RtCommand cmd{};
			cmd.type = Type::AllocateBus;
			cmd.data.bus.busIndex = index;
			cmd.data.bus.parentBusIndex = parentIndex;
			return cmd;
		}

		static RtCommand DeallocateBus(uint32_t index) {
			RtCommand cmd{};
			cmd.type = Type::DeallocateBus;
			cmd.data.bus.busIndex = index;
			return cmd;
		}

		static RtCommand SetBusParent(uint32_t index, uint32_t parentIndex) {
			RtCommand cmd{};
			cmd.type = Type::SetBusParent;
			cmd.data.bus.busIndex = index;
			cmd.data.bus.parentBusIndex = parentIndex;
			return cmd;
		}

		static RtCommand SetBusVolume(uint32_t index, float volume) {
			RtCommand cmd{};
			cmd.type = Type::SetBusVolume;
			cmd.data.busFloat.busIndex = index;
			cmd.data.busFloat.value = volume;
			return cmd;
		}

		static RtCommand AllocateBiquad(uint32_t index, uint32_t gen, BiquadFilterType type, const BiquadConfig& config) {
			RtCommand cmd{};
			cmd.type = Type::AllocateBiquad;
			cmd.data.biquad.index = index;
			cmd.data.biquad.gen = gen;
			cmd.data.biquad.type = type;
			cmd.data.biquad.config = config;
			return cmd;
		}

		static RtCommand SetBiquadParams(uint32_t index, uint32_t gen, const BiquadConfig& config) {
			RtCommand cmd{};
			cmd.type = Type::SetBiquadParams;
			cmd.data.biquad.index = index;
			cmd.data.biquad.gen = gen;
			cmd.data.biquad.config = config;
			return cmd;
		}

		static_assert(std::is_trivially_copyable_v<BiquadConfig>,
			"BiquadConfig must remain trivially copyable for use in RtCommands");

		static RtCommand AttachEffect(uint32_t index, uint32_t gen, EffectType type, uint32_t busIndex,
			uint32_t effectSlot) {
			RtCommand cmd{};
			cmd.type = Type::AttachEffect;
			cmd.data.effect.index = index;
			cmd.data.effect.gen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static RtCommand FadeDetachEffect(uint32_t index, uint32_t gen, EffectType type, uint32_t busIndex,
			uint32_t effectSlot) {
			RtCommand cmd{};
			cmd.type = Type::FadeDetachEffect;
			cmd.data.effect.index = index;
			cmd.data.effect.gen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static RtCommand ForceDetachEffect(uint32_t index, uint32_t gen, EffectType type, uint32_t busIndex,
			uint32_t effectSlot) {
			RtCommand cmd{};
			cmd.type = Type::ForceDetachEffect;
			cmd.data.effect.index = index;
			cmd.data.effect.gen = gen;
			cmd.data.effect.type = type;
			cmd.data.effect.busIndex = busIndex;
			cmd.data.effect.effectSlot = effectSlot;
			return cmd;
		}

		static RtCommand DeallocateEffect(uint32_t index, uint32_t gen, EffectType type) {
			RtCommand cmd{};
			cmd.type = Type::DeallocateEffect;
			cmd.data.effect.index = index;
			cmd.data.effect.gen = gen;
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