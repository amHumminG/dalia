#pragma once
#include "mixer/MixController.h"

#include "async/messaging/AsyncMixerMessaging.h"

namespace dalia {
	template <typename TParams> requires requires (TParams p) { p.Sanitize(); }
	Result MixController::CreateEffect(EffectHandle& effect, const TParams& params) {
		effect = INVALID_EFFECT_HANDLE;

		using Traits = EffectTraits<TParams>;
		auto& ePool = Traits::GetPool(*this);

		uint32_t eIndex = NO_INDEX;
		if (!ePool.Allocate(eIndex)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create effect. Capacity reached.");
			return Result::EffectCapacityReached;
		}

		auto& eMirror = ePool.GetMirror(eIndex);
		uint32_t eGen = eMirror.gen;

		eMirror.params = params;
		eMirror.params.Sanitize();
		ePool.MarkDirty(eIndex);

		effect = EffectHandle::Create(eIndex, eGen, Traits::TYPE);
		m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::AllocateEffect(Traits::TYPE, eIndex, eGen));

		DALIA_LOG_DEBUG(LOG_CTX_API, "Created effect (index: %u, gen: %u).", eIndex, eGen);
		return Result::Ok;
	}

	template <typename TParams> requires requires (TParams p) { p.Sanitize(); }
	Result MixController::SetEffectParams(EffectHandle effect, const TParams& params) {
		return ModifyEffectParams<TParams>(effect, [&](TParams& eParams) -> Result {
			TParams sanitizedParams = params;
			sanitizedParams.Sanitize();

			eParams = sanitizedParams;

			return Result::Ok;
		});
	}

	template<typename TFunc>
	Result MixController::ModifyVoice(PlaybackHandle playback, TFunc&& fn) {
		if (!playback.IsValid()) return Result::InvalidHandle;

		uint32_t vIndex = playback.GetIndex();
		if (vIndex >= m_voices.Capacity()) return Result::InvalidHandle;

		VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
		if (vMirror.gen != playback.GetGeneration()) return Result::ExpiredHandle;

		return fn(vMirror, vIndex);
	}

	template<typename TFunc>
	Result MixController::ModifyVoiceParams(PlaybackHandle playback, TFunc&& fn) {
		return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			fn(vMirror.params);
			m_voices.MarkDirty(vIndex);
			return Result::Ok;
		});
	}

	template<typename TFunc>
	Result MixController::ModifyListener(uint32_t listenerIndex, TFunc&& fn) {
		if (listenerIndex >= m_listeners.Capacity()) return Result::InvalidListener;

		return fn(m_listeners.GetMirror(listenerIndex), listenerIndex);
	}

	template<typename TFunc>
	Result MixController::ModifyListenerParams(uint32_t listenerIndex, TFunc&& fn) {
		return ModifyListener(listenerIndex, [&](ListenerMirror& lMirror, uint32_t lIndex) -> Result {
			fn(lMirror.params);
			m_listeners.MarkDirty(lIndex);
			return Result::Ok;
		});
	}

	template<typename TFunc>
	Result MixController::ModifyBus(const char* identifier, TFunc&& fn) {
		if (!identifier) return Result::InvalidBus;

		const BusID bId{identifier};
		auto it = m_busMap.find(bId);
		if (it == m_busMap.end()) return Result::InvalidBus;

		return fn(m_buses.GetMirror(it->second), it->second);
	}

	template<typename TFunc>
	Result MixController::ModifyBusParams(const char* identifier, TFunc&& fn) {
		return ModifyBus(identifier, [&](BusMirror& bMirror, uint32_t bIndex) -> Result {
			fn(bMirror.params);
			m_buses.MarkDirty(bIndex);
			return Result::Ok;
		});
	}

	template<typename TParams, typename TFunc>
	Result MixController::ModifyEffect(EffectHandle effect, TFunc&& fn) {
		using Traits = EffectTraits<TParams>;
		if (!effect.IsValid() || effect.GetType() != Traits::TYPE) return Result::InvalidHandle;

		auto& ePool = Traits::GetPool(*this);
		uint32_t eIndex = effect.GetIndex();
		if (eIndex >= ePool.Capacity()) return Result::InvalidHandle;

		auto& eMirror = ePool.GetMirror(eIndex);
		if (eMirror.gen != effect.GetGeneration()) return Result::ExpiredHandle;

		return fn(eMirror, eIndex);
	}

	template<typename TFunc>
	Result MixController::ModifyEffect(EffectHandle effect, TFunc&& fn) {
		switch (effect.GetType()) {
			case EffectType::Biquad:
				return ModifyEffect<BiquadParams>(effect, std::forward<TFunc>(fn));
			default:
				return Result::InvalidHandle;
		}
	}

	template<typename TParams, typename TFunc>
	Result MixController::ModifyEffectParams(EffectHandle effect, TFunc&& fn) {
		return ModifyEffect<TParams>(effect, [&](auto& eMirror, uint32_t eIndex) -> Result {
			fn(eMirror.params);
			EffectTraits<TParams>::GetPool(*this).MarkDirty(eIndex);
			return Result::Ok;
		});
	}

	inline auto& EffectTraits<BiquadParams>::GetPool(MixController& controller) {
		return controller.GetBiquadPool();
	}
}
