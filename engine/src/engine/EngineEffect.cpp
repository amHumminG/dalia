#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

namespace dalia {

	template <typename TParams>
	requires requires(TParams p) { p.Sanitize(); }
	Result Engine::CreateEffect(EffectHandle& effect, const TParams& params) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		using Traits = EffectTraits<TParams>;
		auto& ePool = Traits::GetPool(*m_state);

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
		m_state->mixerCommands->Enqueue(MixerCommand::AllocateEffect(Traits::TYPE, eIndex, eGen));

		DALIA_LOG_DEBUG(LOG_CTX_API, "Created effect (index: %u, gen: %u).", eIndex, eGen);
		return Result::Ok;
	}

	template <typename TParams>
	requires requires(TParams p) { p.Sanitize(); }
	Result Engine::SetEffectParams(EffectHandle effect, const TParams& params) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyEffectParams<TParams>(effect, [&](TParams& eParams) -> Result {
			TParams sanitizedParams = params;
			sanitizedParams.Sanitize();

			eParams = sanitizedParams;

			return Result::Ok;
		});
	}

	Result Engine::AttachEffect(EffectHandle effect, const char* busIdentifier, uint32_t effectSlot) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		if (effectSlot >= MAX_EFFECTS_PER_BUS) {
			DALIA_LOG_ERR(LOG_CTX_API,
				"Failed to attach effect to bus %s (slot %d). Valid effect slots are in the range [0, %d].",
				busIdentifier, effectSlot, MAX_EFFECTS_PER_BUS-1);
			return Result::InvalidEffectSlot;
		}

		// Fetch bus
		uint32_t bIndex;
		Result resBus = m_state->GetBusIndex(busIdentifier, bIndex);
		if (resBus != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %d). No bus with identifier %s exists.",
				busIdentifier, effectSlot, busIdentifier);
			return resBus;
		}

		Result res = m_state->ModifyEffect(effect, [&](EffectMirror& eMirror, uint32_t eIndex) -> Result {
			// Steal the effect if it's already attached
			if (eMirror.busIndex != NO_INDEX) {
				uint32_t oldBusIndex = eMirror.busIndex;
				uint32_t oldSlot = eMirror.effectSlot;
				BusMirror& oldBusMirror = m_state->buses.GetMirror(oldBusIndex);
				oldBusMirror.effectSlots[oldSlot] = InvalidEffectHandle;

				m_state->mixerCommands->Enqueue(AsyncMixerCommand::ForceDetachEffect(
					eIndex, eMirror.gen, effect.GetType(), oldBusIndex, oldSlot
				));

				DALIA_LOG_DEBUG(LOG_CTX_API,
					"Attaching effect to %s (index: %u) but effect is already attached. Stealing effect.",
					busIdentifier, bIndex, effectSlot);
			}

			// Check target bus slot and evict old effect if occupied
			BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
			EffectHandle oldEffect = bMirror.effectSlots[effectSlot];
			if (oldEffect.IsValid()) {
				DALIA_LOG_WARN(LOG_CTX_API, "Detaching effect from bus %s (slot %u). Attaching new effect to slot.",
					busIdentifier, effectSlot);

				m_state->DetachEffectFromBus(oldEffect, false);
			}

			// Link the effect to the new bus slot
			bMirror.effectSlots[effectSlot] = effect;
			eMirror.busIndex = bIndex;
			eMirror.effectSlot = effectSlot;

			m_state->mixerCommands->Enqueue(AsyncMixerCommand::AttachEffect(
				eIndex, eMirror.gen, effect.GetType(), bIndex, effectSlot
			));

			DALIA_LOG_DEBUG(LOG_CTX_API, "Attached effect (handle rawId: 0x%016llx), to bus %s (slot %u).",
				effect.GetRawId(), busIdentifier, effectSlot);
			return Result::Ok;
		});

		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %u). Invalid or expired effect handle.",
				busIdentifier, effectSlot);
		}
		return res;
	}

	Result Engine::DetachEffect(EffectHandle effect) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyEffect(effect, [&](EffectMirror& eMirror, uint32_t) -> Result {
			if (eMirror.busIndex == NO_INDEX) {
				DALIA_LOG_WARN(LOG_CTX_API, "Failed to detach effect. Effect is not attached.");
				return Result::Ok;
			}
			m_state->DetachEffectFromBus(effect, true);

			return Result::Ok;
		});

		return m_state->FreeEffectFromPool(effect);
	}

	Result Engine::DestroyEffect(EffectHandle effect) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		Result res = m_state->DetachEffectFromBus(effect, false);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy effect. Invalid or expired handle.");
			return res;
		}

		m_state->ModifyEffect(effect, [&](auto& eMirror, uint32_t eIndex) -> Result {
			m_state->mixerCommands->Enqueue(MixerCommand::FreeEffect(eIndex, eMirror.gen, effect.GetType()));
			eMirror.Reset();
			return Result::Ok;
		});

		if (res != Result::Ok) return res;
		return m_state->FreeEffectFromPool(effect);
	}

	// --- Template Instantiations ---

	// Biquad Filter
	template Result Engine::CreateEffect<BiquadParams>(EffectHandle&, const BiquadParams&);
	template Result Engine::SetEffectParams<BiquadParams>(EffectHandle, const BiquadParams&);
}