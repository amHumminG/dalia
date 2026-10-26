#include "mixer/MixController.h"

namespace dalia {

	Result MixController::DestroyEffect(EffectHandle effect) {
		Result res = DetachEffectFromBus(effect, false);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy effect. Invalid or expired handle.");
			return res;
		}

		ModifyEffect(effect, [&](auto& eMirror, uint32_t eIndex) -> Result {
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::FreeEffect(
				eIndex, eMirror.gen, effect.GetType()
			));
			eMirror.Reset();
			return Result::Ok;
		});

		if (res != Result::Ok) return res;
		return FreeEffectFromPool(effect);
	}

	Result MixController::AttachEffect(EffectHandle effect, const char* bus, uint32_t effectSlot) {
		if (effectSlot >= MAX_EFFECTS_PER_BUS) {
			DALIA_LOG_ERR(LOG_CTX_API,
				"Failed to attach effect to bus %s (slot %d). Valid effect slots are in the range [0, %d].",
				bus, effectSlot, MAX_EFFECTS_PER_BUS - 1);
			return Result::InvalidEffectSlot;
		}

		// Fetch bus
		uint32_t bIndex;
		Result resBus = GetBusIndex(bus, bIndex);
		if (resBus != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %d). No bus with identifier %s exists.",
				bus, effectSlot, bus);
			return resBus;
		}

		Result res = ModifyEffect(effect, [&](EffectMirror& eMirror, uint32_t eIndex) -> Result {
			// Steal the effect if it's already attached
			if (eMirror.busIndex != NO_INDEX) {
				uint32_t oldBusIndex = eMirror.busIndex;
				uint32_t oldSlot = eMirror.effectSlot;
				BusMirror& oldBusMirror = m_buses.GetMirror(oldBusIndex);
				oldBusMirror.effectSlots[oldSlot] = INVALID_EFFECT_HANDLE;

				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::ForceDetachEffect(
					eIndex, eMirror.gen, effect.GetType(), oldBusIndex, oldSlot
				));

				DALIA_LOG_DEBUG(LOG_CTX_API,
					"Attaching effect to %s (index: %u) but effect is already attached. Stealing effect.",
					bus, bIndex, effectSlot);
			}

			// Check target bus slot and evict old effect if occupied
			BusMirror& bMirror = m_buses.GetMirror(bIndex);
			EffectHandle oldEffect = bMirror.effectSlots[effectSlot];
			if (oldEffect.IsValid()) {
				DALIA_LOG_WARN(LOG_CTX_API, "Detaching effect from bus %s (slot %u). Attaching new effect to slot.",
					bus, effectSlot);

				DetachEffectFromBus(oldEffect, false);
			}

			// Link the effect to the new bus slot
			bMirror.effectSlots[effectSlot] = effect;
			eMirror.busIndex = bIndex;
			eMirror.effectSlot = effectSlot;

			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::AttachEffect(
				eIndex, eMirror.gen, effect.GetType(), bIndex, effectSlot
			));

			DALIA_LOG_DEBUG(LOG_CTX_API, "Attached effect (handle rawId: 0x%016llx), to bus %s (slot %u).",
				effect.GetRawId(), bus, effectSlot);
			return Result::Ok;
		});

		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %u). Invalid or expired effect handle.",
				bus, effectSlot);
		}
		return res;
	}

	Result MixController::DetachEffect(EffectHandle effect) {
		return ModifyEffect(effect, [&](EffectMirror& eMirror, uint32_t) -> Result {
			if (eMirror.busIndex == NO_INDEX) {
				DALIA_LOG_WARN(LOG_CTX_API, "Failed to detach effect. Effect is not attached.");
				return Result::Ok;
			}
			DetachEffectFromBus(effect, true);

			return Result::Ok;
		});

		return FreeEffectFromPool(effect);
	}
}
