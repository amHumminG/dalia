#include "mixer/MixController.h"

#include "asset/AssetController.h"
#include "io/IoController.h"

namespace dalia {

	Result MixController::CreatePlayback(PlaybackHandle& playback, SoundHandle sound,
		PlaybackExitCallback exitCallback) {
		if (!m_assetController->AcquireSound(sound)) return Result::InvalidHandle;

		SoundDescriptor soundDesc{};
		Result res = m_assetController->GetSoundDescriptor(sound, soundDesc);
		if (m_assetController->GetSoundDescriptor(sound, soundDesc) != Result::Ok) return res;

		if (soundDesc.loadState == LoadState::Error) {
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Failed to play sound. Sound loading failed.");
			return Result::SoundLoadError;
			m_assetController->ReleaseSound(sound);
		}

		// Sound exists -> Allocate a voice
		uint32_t vIndex;
		if (!m_voices.Allocate(vIndex)) {
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Failed to create playback instance. Voice pool capacity reached.");
			return Result::VoiceCapacityReached;
			m_assetController->ReleaseSound(sound);
		}

		// Prime voice mirror
		VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
		vMirror.state = VoiceState::Inactive;
		vMirror.assetRawId = sound.GetRawId();
		vMirror.totalFrames = soundDesc.totalFrames;
		vMirror.channels = soundDesc.channels;
		vMirror.sampleRate = soundDesc.sampleRate;
		vMirror.soundType = soundDesc.soundType;
		if (exitCallback) vMirror.onStopCallback = std::move(exitCallback);

		if (soundDesc.loadState == LoadState::Loading) {
			playback = PlaybackHandle::Create(vIndex, vMirror.gen);
			m_pendingPlaybacks.push_back({.playback = playback, .assetRawId = sound.GetRawId()});
			vMirror.pendingLoad = true;
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Deferring playback for voice %u. Sound is still loading.", vIndex);
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::AllocateVoice(vIndex, vMirror.gen));
			return Result::Ok;
		}

		if (soundDesc.soundType == SoundType::Resident) {
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::AllocateVoice(vIndex, vMirror.gen));
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PrepareVoiceResident(
				vIndex, vMirror.gen, soundDesc.pcmData, soundDesc.totalFrames, soundDesc.channels, soundDesc.sampleRate
			));
		}
		else if (soundDesc.soundType == SoundType::Stream) {
			uint32_t sIndex, sGen;
			res = m_ioController->AllocateAndPrepareStream(soundDesc.filepath, sIndex, sGen);
			if (res != Result::Ok) {
				ResetAndFreeVoice(vIndex);

				if (res == Result::StreamCapacityReached) {
					DALIA_LOG_ERR(LOG_CTX_API, "Failed to prepare stream instance. Stream capacity reached.");
				}
				if (res == Result::StreamQueueCapacityReached) {
					DALIA_LOG_ERR(LOG_CTX_API, "Failed to prepare stream instance. Stream queue capacity reached.");
				}

				return res;
			}

			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::AllocateVoice(vIndex, vMirror.gen));
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PrepareVoiceStreaming(
				vIndex, vMirror.gen, sIndex, sGen, soundDesc.channels, soundDesc.sampleRate
			));
		}

		playback = PlaybackHandle::Create(vIndex, vMirror.gen);
		DALIA_LOG_DEBUG(LOG_CTX_CORE, "Created playback instance using voice %u.", vIndex);
		return Result::Ok;
	}

	Result MixController::RoutePlayback(PlaybackHandle playback, const char* bus) {
		return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			uint32_t bIndex;
			Result res = GetBusIndex(bus, bIndex);
			if (res != Result::Ok) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to route playback. No bus with identifier %s exists.", bus);
				return res;
			}
			if (vMirror.parentBusIndex == bIndex) {
				DALIA_LOG_WARN(LOG_CTX_API,
					"Attempting to route playback to %s. Playback is already routed to the specified bus.", bus);
				return Result::Ok;
			}

			vMirror.parentBusIndex = bIndex;
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::SetVoiceParent(
				vIndex, playback.GetGeneration(), bIndex
			));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Routed voice %d to bus %s (index: %d).", vIndex, bus, bIndex);

			return Result::Ok;
		});
	}

	Result MixController::PlayPlayback(PlaybackHandle playback) {
		return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
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
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PlayVoice(vIndex, playback.GetGeneration()));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d state: Playing.", vIndex);

			return Result::Ok;
		});
	}

	Result MixController::PausePlayback(PlaybackHandle playback) {
		return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			if (vMirror.state != VoiceState::Playing) {
				DALIA_LOG_WARN(LOG_CTX_API, "Calling pause on playback that is not currently playing.");
				return Result::Ok;
			}

			vMirror.state = VoiceState::Paused;
			if (vMirror.pendingLoad) return Result::Ok; // Do not send command if voice is pending load

			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PauseVoice(vIndex, playback.GetGeneration()));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d state: Paused.", vIndex);

			return Result::Ok;
		});
	}

	Result MixController::StopPlayback(PlaybackHandle playback) {
		return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			// Handle pending playbacks
			if (vMirror.pendingLoad) {
				for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end(); ) {
					if (it->playback.GetIndex() == vIndex) {
						if (vMirror.onStopCallback) {
							vMirror.onStopCallback(playback, PlaybackExitCondition::ExplicitStop);
						}

						vMirror.Reset();
						m_voices.Free(vIndex);
						DALIA_LOG_DEBUG(LOG_CTX_API, "Stopped voice %d before it started playing.", vIndex);
						m_pendingPlaybacks.erase(it);
						return Result::Ok;
					}
					it++;
				}
			}

			vMirror.state = VoiceState::Stopped;

			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::StopVoice(vIndex, playback.GetGeneration()));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to stop.", vIndex);

			return Result::Ok;
		});
	}

	void MixController::StopPlaybacksForSound(SoundHandle sound) {
		uint64_t soundRawId = sound.GetRawId();

		// Free pending playbacks
		for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end();) {
			if (it->assetRawId == soundRawId) {
				uint32_t vIndex = it->playback.GetIndex();
				VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
				if (vMirror.gen == it->playback.GetGeneration()) ResetAndFreeVoice(vIndex);
				it = m_pendingPlaybacks.erase(it);
			}
			else {
				++it;
			}
		}

		// Command active voices using this sound to stop
		for (uint32_t i = 0; i < m_voices.Capacity(); ++i) {
			VoiceMirror& vMirror = m_voices.GetMirror(i);
			if (vMirror.gen != NO_GENERATION && vMirror.assetRawId == soundRawId) {
				vMirror.state = VoiceState::Stopped;
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::StopVoice(i, vMirror.gen));
			}
		}
	}

	Result MixController::SeekPlayback(PlaybackHandle playback, double timeInSeconds) {
		return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
			if (timeInSeconds < 0.0) timeInSeconds = 0.0;

			if (vMirror.pendingLoad) {
				// Voice is pending
				for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end(); ) {
					if (it->playback.GetIndex() == vIndex) {
						vMirror.initialSeekTarget = timeInSeconds;
						return Result::Ok;
					}
					++it;
				}
			}

			auto seekFrame64 = static_cast<uint64_t>(timeInSeconds * vMirror.sampleRate);
			if (seekFrame64 >= vMirror.totalFrames) { // Safety clamps
				DALIA_LOG_WARN(LOG_CTX_API, "Seeking beyond final sound frame. Clamping to final frame.");
				if (vMirror.totalFrames > 0) seekFrame64 = vMirror.totalFrames - 1;
				else seekFrame64 = 0;
			}

			auto seekFrame = static_cast<uint32_t>(seekFrame64);
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::SeekVoice(
				vIndex, playback.GetGeneration(), seekFrame
			));
			DALIA_LOG_DEBUG(LOG_CTX_API, "Seeking voice %d to frame %d.", vIndex, seekFrame);

			return Result::Ok;
		});
	}

	Result MixController::SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedVolumeDb = std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX);
			params.gain = math::DbToGain(clampedVolumeDb);
		});
	}

	Result MixController::SetPlaybackRate(PlaybackHandle playback, float rate) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedRate = std::clamp(rate, PLAYBACK_RATE_MIN, PLAYBACK_RATE_MAX);
			params.playbackRate = clampedRate;
		});
	}

	Result MixController::SetPlaybackStereoPan(PlaybackHandle playback, float stereoPan) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedStereoPan = std::clamp(stereoPan, PAN_STEREO_MIN, PAN_STEREO_MAX);
			params.stereoPan = clampedStereoPan;
		});
	}

	Result MixController::SetPlaybackLooping(PlaybackHandle playback, bool looping) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.isLooping = looping;
		});
	}

	Result MixController::SetPlaybackSpatial(PlaybackHandle playback, bool spatial) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.isSpatial = spatial;
		});
	}

	Result MixController::SetPlaybackPosition(PlaybackHandle playback, math::Vector3 position) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.position = position;
		});
	}

	Result MixController::SetPlaybackVelocity(PlaybackHandle playback, math::Vector3 velocity) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.velocity = velocity;
		});
	}

	Result MixController::SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode distanceMode) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.distanceMode = distanceMode;
		});
	}

	Result MixController::SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.attenuationCurve = curve;
		});
	}

	Result MixController::SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			float clampedMinDistance = std::max(minDistance, MIN_DIST_MIN);
			float clampedMaxDistance = std::max(maxDistance, clampedMinDistance);
			params.minDistance = clampedMinDistance;
			params.maxDistance = clampedMaxDistance;
		});
	}

	Result MixController::SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.useDoppler = useDoppler;
		});
	}

	Result MixController::SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.dopplerFactor = std::clamp(dopplerFactor, DOPPLER_FACTOR_MIN, DOPPLER_FACTOR_MAX);
		});
	}

	Result MixController::SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask listenerMask) {
		return ModifyVoiceParams(playback, [&](VoiceParams& params) {
			params.listenerMask = listenerMask;
		});
	}
}
