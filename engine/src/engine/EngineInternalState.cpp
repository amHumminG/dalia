#include "engine/EngineInternalState.h"

#include "backend/DeviceManager.h"
#include "backend/NullOutputDevice.h"

#include "async/AsyncControlSystem.h"
#include "async/AsyncLoadSystem.h"
#include "async/AsyncStreamSystem.h"

#include "resource/AssetRegistry.h"

#include "mixer/AsyncMixerSystem.h"
#include "mixer/MixGraphCompiler.h"
#include "mixer/StreamContext.h"

#include <algorithm>

namespace dalia {

	EngineInternalState::EngineInternalState(const EngineConfig& config)
		: asyncDirector(config.advanced.mixerQueueCapacity, config.advanced.streamQueueCapacity,
			config.advanced.loadQueueCapacity, config.advanced.controlQueueCapacity),
		coordinateSystem(config.coordinateSystem), streams(config.streamCapacity), voices(config.voiceCapacity),
		listeners(std::clamp(config.listenerCapacity, LISTENERS_MIN, LISTENERS_MAX)),
		buses(config.busCapacity), biquads(config.biquadCapacity) {

		// Mixing
		mixGraphCompiler	= std::make_unique<MixGraphCompiler>(config.busCapacity);
		mixOrder			= std::make_unique<uint32_t[]>(config.busCapacity);

		// Resources
		assetRegistry	= std::make_unique<AssetRegistry>(config.residentSoundCapacity, config.streamSoundCapacity);
	}

	bool EngineInternalState::IsInitialized(const EngineInternalState* state) {
		return state != nullptr && state->initialized;
	}

	uint32_t EngineInternalState::GenerateIoLoadRequestId() {
		return nextIoLoadRequestId++;
	}

	PlaybackHandle EngineInternalState::ForgePlaybackHandle(uint32_t index, uint32_t gen) {
		return PlaybackHandle::Create(index, gen);
	}

	SoundHandle EngineInternalState::ForgeSoundHandle(uint64_t rawId) {
		return SoundHandle::FromRawId(rawId);
	}

	EffectHandle EngineInternalState::ForgeEffectHandle(uint64_t rawId) {
		return EffectHandle::FromRawId(rawId);
	}

	uint32_t EngineInternalState::GetPlaybackIndex(PlaybackHandle playback) {
		return playback.GetIndex();
	}

	uint32_t EngineInternalState::GetPlaybackGeneration(PlaybackHandle playback) {
		return playback.GetGeneration();
	}

	uint32_t EngineInternalState::GetEffectIndex(EffectHandle effect) {
		return effect.GetIndex();
	}

	uint32_t EngineInternalState::GetEffectGen(EffectHandle effect) {
		return effect.GetGeneration();
	}

	Result EngineInternalState::Validate(PlaybackHandle playback) {
		return ModifyVoice(playback, [](const VoiceMirror&, uint32_t) { return Result::Ok; });
	}

	Result EngineInternalState::Validate(uint32_t listenerIndex) {
		return ModifyListener(listenerIndex, [](const ListenerMirror&, uint32_t) { return Result::Ok; });
	}

	Result EngineInternalState::Validate(const char* busIdentifier) {
		return ModifyBus(busIdentifier, [](const BusMirror&, uint32_t) { return Result::Ok; });
	}

	Result EngineInternalState::Validate(EffectHandle effect) {
		return ModifyEffect(effect, [](const EffectMirror&, uint32_t) { return Result::Ok; });
	}

	Result EngineInternalState::FreeEffectFromPool(EffectHandle effect) {
		uint32_t eIndex = effect.GetIndex();
		switch (effect.GetType()) {
			case EffectType::Biquad:
				if (eIndex >= biquads.Capacity()) return Result::InvalidHandle;
				biquads.Free(eIndex);
				return Result::Ok;
			default:
				return Result::InvalidHandle;
		}
	}

	Result EngineInternalState::DetachEffectFromBus(EffectHandle effect, bool fadeOut) {
		return ModifyEffect(effect, [&](EffectMirror& eMirror, uint32_t) -> Result {
			if (!eMirror.IsAttached()) return Result::Ok;

			uint32_t bIndex = eMirror.busIndex;
			uint32_t slot = eMirror.effectSlot;

			BusMirror& bMirror = buses.GetMirror(bIndex);
			if (bMirror.effectSlots[slot] == effect) bMirror.effectSlots[slot] = InvalidEffectHandle;

			eMirror.busIndex = NO_INDEX;
			eMirror.effectSlot = NO_INDEX;

			if (fadeOut) {
				mixerCommands->Enqueue(AsyncMixerCommand::FadeDetachEffect(
					effect.GetIndex(), eMirror.gen, effect.GetType(), bIndex, slot
				));
			}
			else {
				mixerCommands->Enqueue(AsyncMixerCommand::ForceDetachEffect(
					effect.GetIndex(), eMirror.gen, effect.GetType(), bIndex, slot
				));
			}

			return Result::Ok;
		});
	}
}
