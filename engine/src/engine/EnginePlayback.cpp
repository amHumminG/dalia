#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"
#include "core/Math.h"

namespace dalia {

	Result Engine::CreatePlayback(PlaybackHandle& playback, SoundHandle sound, PlaybackExitCallback exitCallback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->CreatePlayback(playback, sound, std::move(exitCallback));
	}

	Result Engine::RoutePlayback(PlaybackHandle playback, const char* bus) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->RoutePlayback(playback, bus);
	}

	Result Engine::PlayPlayback(PlaybackHandle playback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->PlayPlayback(playback);
	}

	Result Engine::PausePlayback(PlaybackHandle playback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->PausePlayback(playback);
	}

	Result Engine::StopPlayback(PlaybackHandle playback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->StopPlayback(playback);
	}

	Result Engine::SeekPlayback(PlaybackHandle playback, double timeInSeconds) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SeekPlayback(playback, timeInSeconds);
	}

	Result Engine::SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackVolumeDb(playback, volumeDb);
	}

	Result Engine::SetPlaybackRate(PlaybackHandle playback, float rate) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackRate(playback, rate);
	}

	Result Engine::SetPlaybackStereoPan(PlaybackHandle playback, float stereoPan) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackStereoPan(playback, stereoPan);
	}

	Result Engine::SetPlaybackLooping(PlaybackHandle playback, bool looping) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackLooping(playback, looping);
	}

	Result Engine::SetPlaybackSpatial(PlaybackHandle playback, bool spatial) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackSpatial(playback, spatial);
	}

	Result Engine::SetPlaybackPosition(PlaybackHandle playback, const Vec3& position) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackPosition(playback, math::Vector3::FromPublic(position));
	}

	Result Engine::SetPlaybackVelocity(PlaybackHandle playback, const Vec3& velocity) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackVelocity(playback, math::Vector3::FromPublic(velocity));
	}

	Result Engine::SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode distanceMode) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackDistanceMode(playback, distanceMode);
	}

	Result Engine::SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackAttenuationCurve(playback, curve);
	}

	Result Engine::SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackMinMaxDistance(playback, minDistance, maxDistance);
	}

	Result Engine::SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackUseDoppler(playback, useDoppler);
	}

	Result Engine::SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackDopplerFactor(playback, dopplerFactor);
	}

	Result Engine::SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask listenerMask) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetPlaybackListenerMask(playback, listenerMask);
	}
}
