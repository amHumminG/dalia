#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"
#include "resources/AssetRegistry.h"
#include "core/Math.h"

#include <algorithm>

namespace dalia {

	Result Engine::CreatePlayback(PlaybackHandle& pbkHandle, SoundHandle soundHandle, PlaybackExitCallback callback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = soundHandle.GetType();
		ResidentSound* residentSound = nullptr;
		StreamSound* streamSound = nullptr;
		LoadState soundLoadState;
		uint32_t frameCount = 0;
		uint32_t channels = 0;
		uint32_t sampleRate = 0;

		if (soundType == SoundType::Resident) {
			residentSound = m_state->assetRegistry->GetResidentSound(soundHandle);
			if (!residentSound) return Result::InvalidHandle;
			soundLoadState = residentSound->state.load(std::memory_order_acquire);
			frameCount = residentSound->frameCount;
			channels = residentSound->channels;
			sampleRate = residentSound->sampleRate;
		}
		else {
			streamSound = m_state->assetRegistry->GetStreamSound(soundHandle);
			if (!streamSound) return Result::InvalidHandle;
			soundLoadState = streamSound->state.load(std::memory_order_acquire);
			frameCount = streamSound->frameCount;
			channels = streamSound->channels;
			sampleRate = streamSound->sampleRate;
		}

		if (soundLoadState == LoadState::Error) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to play sound. Sound loading failed.");
			return Result::SoundLoadError;
		}

		uint32_t vIndex;
		if (!m_state->voices.Allocate(vIndex)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create playback instance. Voice pool capacity reached.");
			return Result::VoiceCapacityReached;
		}

		// Prime voice mirror
		VoiceMirror& vMirror = m_state->voices.GetMirror(vIndex);
		vMirror.state = VoiceState::Inactive;
		vMirror.assetRawId = soundHandle.GetRawId();
		vMirror.frameCount = frameCount;
		vMirror.channels = channels;
		vMirror.sampleRate = sampleRate;
		vMirror.soundType = soundType;
		if (callback) vMirror.onStopCallback = callback;

		// Deferred playback
		if (soundLoadState == LoadState::Loading) {
			m_state->pendingPlaybacks.push_back({
				EngineInternalState::ForgePlaybackHandle(vIndex, vMirror.gen),
				soundHandle.GetRawId()
			});
			vMirror.pendingLoad = true;
			DALIA_LOG_DEBUG(LOG_CTX_API, "Deferring playback for voice %d. Sound not yet loaded.", vIndex);

			pbkHandle = PlaybackHandle::Create(vIndex, vMirror.gen);
			m_state->mixerCommands->Enqueue(MixerCommand::AllocateVoice(vIndex, vMirror.gen));
			return Result::Ok;
		}

		if (soundType == SoundType::Resident) {
			m_state->mixerCommands->Enqueue(MixerCommand::AllocateVoice(vIndex, vMirror.gen));
			MixerCommand cmd = MixerCommand::PrepareVoiceResident(
				vIndex,
				vMirror.gen,
				residentSound->pcmData.data(),
				residentSound->frameCount,
				residentSound->channels,
				residentSound->sampleRate
			);
			m_state->mixerCommands->Enqueue(cmd);
		}
		else if (soundType == SoundType::Stream) {
			uint32_t sIndex;
			uint32_t sGen;
			Result streamRes = DispatchStreamPrepare(m_state, streamSound->filepath, sIndex, sGen);
			if (streamRes != Result::Ok) {
				vMirror.Reset();
				m_state->voices.Free(vIndex);

				if (streamRes == Result::StreamCapacityReached) {
					DALIA_LOG_ERR(LOG_CTX_API, "Failed to prepare stream instance. Stream pool capacity reached.");
				}
				if (streamRes == Result::IoStreamRequestQueueFull) {
					DALIA_LOG_ERR(LOG_CTX_API, "Failed to prepare stream instance. I/O stream request queue full.");
				}

				return streamRes;
			}

			m_state->mixerCommands->Enqueue(MixerCommand::AllocateVoice(vIndex, vMirror.gen));
			MixerCommand cmd = MixerCommand::PrepareVoiceStreaming(
				vIndex,
				vMirror.gen,
				sIndex,
				sGen,
				streamSound->channels,
				streamSound->sampleRate
			);
			m_state->mixerCommands->Enqueue(cmd);
		}

		pbkHandle = PlaybackHandle::Create(vIndex, vMirror.gen);
		DALIA_LOG_DEBUG(LOG_CTX_API, "Created playback instance using voice %d.", vIndex);

		return Result::Ok;
	}

	Result Engine::RoutePlayback(PlaybackHandle playback, const char* busIdentifier) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			uint32_t bIndex;
			Result res = m_state->GetBusIndex(busIdentifier, bIndex);
			if (res != Result::Ok) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to route playback. No bus with identifier %s exists.", busIdentifier);
				return res;
			}
			if (vMirror.parentBusIndex == bIndex) {
				DALIA_LOG_WARN(LOG_CTX_API,
					"Attempting to route playback to %s. Playback is already routed to the specified bus.",
					busIdentifier);
				return Result::Ok;
			}

			vMirror.parentBusIndex = bIndex;
			m_state->mixerCommands->Enqueue(MixerCommand::SetVoiceParent(vIndex, playback.GetGeneration(), bIndex));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Routed voice %d to bus %s (index: %d).", vIndex, busIdentifier, bIndex);

			return Result::Ok;
		});
	}

	Result Engine::PlayPlayback(PlaybackHandle playback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			if (vMirror.pendingLoad) {
			   vMirror.state = VoiceState::Playing;
			   DALIA_LOG_DEBUG(LOG_CTX_API, "Calling play on sound that is still loading. Will play when done loading.");
			   return Result::Ok;
			}

			// Check for double calls
			if (vMirror.state == VoiceState::Playing) {
				DALIA_LOG_WARN(LOG_CTX_API, "Calling play on playback that is already set to play.");
				return Result::Ok;
			}

			if (vMirror.state != VoiceState::Inactive && vMirror.state != VoiceState::Paused) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to start playback. Playback corrupted.");
				DALIA_LOG_DEBUG(LOG_CTX_API, "Failed to play voice (index: %d). Mirror state corrupted (State: %d).",
					vIndex, static_cast<int>(vMirror.state));
				return Result::PlaybackCorrupted;
			}

			vMirror.state = VoiceState::Playing;
			m_state->mixerCommands->Enqueue(MixerCommand::PlayVoice(vIndex, playback.GetGeneration()));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d state: Playing.", vIndex);

			return Result::Ok;
		});
	}

	Result Engine::PausePlayback(PlaybackHandle playback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			if (vMirror.state != VoiceState::Playing) {
				DALIA_LOG_WARN(LOG_CTX_API, "Calling pause on playback that is not currently playing.");
				return Result::Ok;
			}

			vMirror.state = VoiceState::Paused;
			if (vMirror.pendingLoad) return Result::Ok; // Do not send command if voice is pending load

			m_state->mixerCommands->Enqueue(MixerCommand::PauseVoice(vIndex, playback.GetGeneration()));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d state: Paused.", vIndex);

			return Result::Ok;
		});
	}

	Result Engine::StopPlayback(PlaybackHandle playback) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			// Handle pending playbacks
			if (vMirror.pendingLoad) {
				for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
					if (it->playback.GetIndex() == vIndex) {
						if (vMirror.onStopCallback) {
							vMirror.onStopCallback(playback, PlaybackExitCondition::ExplicitStop);
						}

						vMirror.Reset();
						m_state->voices.Free(vIndex);
						DALIA_LOG_DEBUG(LOG_CTX_API, "Stopped voice %d before it started playing.", vIndex);
						m_state->pendingPlaybacks.erase(it);
						return Result::Ok;
					}
					it++;
				}
			}

			vMirror.state = VoiceState::Stopped;

			m_state->mixerCommands->Enqueue(MixerCommand::StopVoice(vIndex, playback.GetGeneration()));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to stop.", vIndex);

			return Result::Ok;
		});
	}

	Result Engine::SeekPlayback(PlaybackHandle playback, double timeInSeconds) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			if (timeInSeconds < 0.0) timeInSeconds = 0.0;

			uint64_t seekFrame64 = static_cast<uint64_t>(timeInSeconds * vMirror.sampleRate);

			// Safety clamps
			if (seekFrame64 >= vMirror.frameCount) {
				DALIA_LOG_WARN(LOG_CTX_API, "Seeking beyond final sound frame. Clamping to final frame.");
				if (vMirror.frameCount > 0) seekFrame64 = vMirror.frameCount - 1;
				else seekFrame64 = 0;
			}

			uint32_t seekFrame = static_cast<uint32_t>(seekFrame64);
			m_state->mixerCommands->Enqueue(MixerCommand::SeekVoice(vIndex, playback.GetGeneration(), seekFrame));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Seeking voice %d to frame %d.", vIndex, seekFrame);

			return Result::Ok;
		});
	}

	Result Engine::SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedVolumeDb = std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX);
			params.gain = math::DbToGain(clampedVolumeDb);
		});
	}

	Result Engine::SetPlaybackRate(PlaybackHandle playback, float rate) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedRate = std::clamp(rate, PLAYBACK_RATE_MIN, PLAYBACK_RATE_MAX);
			params.playbackRate = clampedRate;
		});
	}

	Result Engine::SetPlaybackStereoPan(PlaybackHandle playback, float pan) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedStereoPan = std::clamp(pan, PAN_STEREO_MIN, PAN_STEREO_MAX);
			params.stereoPan = clampedStereoPan;
		});
	}

	Result Engine::SetPlaybackLooping(PlaybackHandle playback, bool looping) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.isLooping = looping;
		});
	}

	Result Engine::SetPlaybackSpatial(PlaybackHandle playback, bool spatial) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.isSpatial = spatial;
		});
	}

	Result Engine::SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode mode) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.distanceMode = mode;
		});
	}

	Result Engine::SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.attenuationCurve = curve;
		});
	}

	Result Engine::SetPlaybackPosition(PlaybackHandle playback, const Vec3& position) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.position = math::Vector3::FromPublic(position);
		});
	}

	Result Engine::SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedMinDistance = std::max(minDistance, MIN_DIST_MIN);
			float clampedMaxDistance = std::max(maxDistance, clampedMinDistance);
			params.minDistance = clampedMinDistance;
			params.maxDistance = clampedMaxDistance;
		});
	}

	Result Engine::SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.useDoppler = useDoppler;
		});
	}

	Result Engine::SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.dopplerFactor = std::clamp(dopplerFactor, DOPPLER_FACTOR_MIN, DOPPLER_FACTOR_MAX);
		});
	}

	Result Engine::SetPlaybackVelocity(PlaybackHandle playback, const Vec3& velocity) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.velocity = math::Vector3::FromPublic(velocity);
		});
	}

	Result Engine::SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask mask) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.listenerMask = mask;
		});
	}
}
