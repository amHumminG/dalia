#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

namespace dalia {

	Result Engine::LoadSoundAsync(SoundHandle& sound, SoundType soundType, const char* filepath,
		AssetLoadCallback loadCallback, uint32_t* requestId) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->assetController->LoadSoundAsync(sound, soundType, filepath, std::move(loadCallback),
			requestId);
	}

	Result Engine::UnloadSound(SoundHandle sound) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		Result res = m_state->assetController->UnloadSound(sound);
		if (res != Result::Ok) return res;

		m_state->mixController->StopPlaybacksForSound(sound);
		return Result::Ok;
	}

	Result Engine::GetSoundLength(SoundHandle sound, double& lengthInSeconds) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		SoundDescriptor soundDesc{};
		Result res = m_state->assetController->GetSoundDescriptor(sound, soundDesc);
		if (res != Result::Ok) return res;
		if (soundDesc.loadState != LoadState::Loaded) return Result::SoundNotLoaded;
		lengthInSeconds = soundDesc.lengthInSeconds;

		return Result::Ok;
	}
}
