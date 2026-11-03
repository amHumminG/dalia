#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

namespace dalia {

	template <typename TParams>
	Result Engine::CreateEffect(EffectHandle& effect, const TParams& params) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->CreateEffect<TParams>(effect, params);
	}

	Result Engine::DestroyEffect(EffectHandle effect) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->DestroyEffect(effect);
	}

	Result Engine::AttachEffect(EffectHandle effect, const char* bus, uint32_t effectSlot) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->AttachEffect(effect, bus, effectSlot);
	}

	Result Engine::DetachEffect(EffectHandle effect) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->DetachEffect(effect);
	}

	template <typename TParams>
	Result Engine::SetEffectParams(EffectHandle effect, const TParams& params) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetEffectParams<TParams>(effect, params);
	}

	// --- Template Instantiations ---

	// Biquad Filter
	template Result Engine::CreateEffect<BiquadParams>(EffectHandle&, const BiquadParams&);
	template Result Engine::SetEffectParams<BiquadParams>(EffectHandle, const BiquadParams&);
}