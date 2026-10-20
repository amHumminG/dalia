#include "async/AsyncDirector.h"

#include "engine/EngineInternalState.h"
#include "resource/AssetRegistry.h"

#include "resource/StreamSound.h"
#include "resource/ResidentSound.h"

namespace dalia {
	AsyncDirector::AsyncDirector(size_t mixerQueueCapacity, size_t streamQueueCapacity, size_t loadQueueCapacity,
		size_t controlQueueCapacity)
			: m_mixerCommandQueue(mixerQueueCapacity), m_mixerEventQueue(mixerQueueCapacity),
			m_streamCommandQueue(streamQueueCapacity),
			m_loadCommandQueue(loadQueueCapacity), m_loadEventQueue(loadQueueCapacity),
			m_controlCommandQueue(controlQueueCapacity), m_controlEventQueue(controlQueueCapacity) {
	}

	void AsyncDirector::UpdateMain(EngineInternalState* state) {
		CatchMixerEvents(state);
		CatchLoadEvents(state);
		CatchControlEvents(state);

		m_mixerCommandQueue.Dispatch();
	}

	void AsyncDirector::PushMixerCommand(const AsyncMixerCommand& command) {
		m_mixerCommandQueue.Enqueue(command);
	}

	void AsyncDirector::PushStreamCommand(const AsyncStreamCommand& command) {
		m_streamCommandQueue.Push(command);
	}

	void AsyncDirector::PushLoadCommand(const AsyncLoadCommand& command) {
		m_loadCommandQueue.Push(command);
	}

	void AsyncDirector::PushControlCommand(const AsyncControlCommand& command) {
		m_controlCommandQueue.Push(command);
	}

	void AsyncDirector::PushMixerEvent(const AsyncMixerEvent& event) {
		m_mixerEventQueue.Push(event);
	}

	void AsyncDirector::PushLoadEvent(const AsyncLoadEvent& event) {
		m_loadEventQueue.Push(event);
	}

	void AsyncDirector::PushControlEvent(const AsyncControlEvent& event) {
		m_controlEventQueue.Push(event);
	}

	void AsyncDirector::CatchMixerEvents(EngineInternalState& state) {
		AsyncMixerEvent ev;
		while (m_mixerEventQueue.Pop(ev)) {
			switch (ev.type) {
				case AsyncMixerEvent::Type::VoiceStopped: {
					uint32_t vIndex = ev.data.voice.index;
					uint32_t vGen = ev.data.voice.generation;
					PlaybackHandle forgedHandle = state->ForgePlaybackHandle(vIndex, vGen);

					Result res = state->ModifyVoice(forgedHandle, [&](VoiceMirror& vMirror, uint32_t) -> Result {
						PlaybackExitCallback callback = vMirror.onStopCallback;
						vMirror.Reset();
						state->voices.Free(vIndex);

						DALIA_LOG_DEBUG(LOG_CTX_CORE, "Freed voice %d.", vIndex);
						if (callback) callback(forgedHandle, ev.data.voice.exitCondition);

						return Result::Ok;
					});

					if (res == Result::Ok) {
						// --- Check garbage collection ---
						PlaybackHandle stopped = EngineInternalState::ForgePlaybackHandle(vIndex, vGen);
						for (auto it = state->pendingSoundUnloads.begin(); it != state->pendingSoundUnloads.end(); ) {
							auto& waitingList = it->voicesToStop;

							for (size_t i = 0; i < waitingList.size(); i++) {
								if (waitingList[i] == stopped) {
									waitingList[i] = waitingList.back();
									waitingList.pop_back();
									break;
								}
							}

							if (waitingList.empty()) {
								state->assetRegistry->FreeSound(it->handle);

								const char* typeStr = (it->handle.GetType() == SoundType::Resident) ? "resident" : "stream";
								DALIA_LOG_DEBUG(LOG_CTX_CORE, "Unloaded %s sound with handle %d (deferred)",
									typeStr, it->handle.GetRawId());

								it = state->pendingSoundUnloads.erase(it);
							}
							else it++;
						}
					}
					break;
				}
				default: break;
			}
		}
	}

	void AsyncDirector::CatchLoadEvents(EngineInternalState* state) {
		AsyncLoadEvent ev;
		while (m_loadEventQueue.Pop(ev)) {
			// Execute user-registered callback
			if (auto it = state->loadCallbacks.find(ev.requestId); it != state->loadCallbacks.end()) {
				if (it->second) {
					it->second(ev.requestId, ev.result); // Call it
				}
				state->loadCallbacks.erase(it);
			}

			switch (ev.type) {
			case AsyncLoadEvent::Type::SoundLoaded: {
				// Process deferred playbacks
				for (auto it = state->pendingPlaybacks.begin(); it != state->pendingPlaybacks.end(); ) {
					if (it->assetRawId == ev.assetRawId) {
						uint32_t vIndex = EngineInternalState::GetPlaybackIndex(it->playback);
						uint32_t vGen = EngineInternalState::GetPlaybackGeneration(it->playback);
						Result res = state->Validate(it->playback);
						VoiceMirror& vMirror = state->voices.GetMirror(vIndex);

						if (res == Result::Ok && vMirror.pendingLoad) {
							SoundHandle handle = state->ForgeSoundHandle(ev.assetRawId);
							SoundType soundType = handle.GetType();
							uint32_t frameCount = 0;
							uint32_t channels = 0;
							uint32_t sampleRate = 0;

							if (soundType == SoundType::Resident) {
								ResidentSound* sound = state->assetRegistry->GetResidentSound(handle);

								AsyncMixerCommand cmd = AsyncMixerCommand::PrepareVoiceResident(
									vIndex,
									vGen,
									sound->pcmData.data(),
									sound->frameCount,
									sound->channels,
									sound->sampleRate
								);
								state->mixerCommands->Enqueue(cmd);

								frameCount = sound->frameCount;
								channels = sound->channels;
								sampleRate = sound->sampleRate;
							}
							else if (soundType == SoundType::Stream) {
								StreamSound* sound = state->assetRegistry->GetStreamSound(handle);

								uint32_t sIndex;
								uint32_t sGen;
								Result streamResult = DispatchStreamPrepare(state, sound->filepath, sIndex, sGen);
								if (streamResult != Result::Ok) {
									DALIA_LOG_ERR(LOG_CTX_CORE,
										"Aborting deferred playback. Stream preparation failed.");

									if (vMirror.onStopCallback) {
										PlaybackHandle playback = state->ForgePlaybackHandle(
											vIndex,
											vGen
										);
										vMirror.onStopCallback(playback, PlaybackExitCondition::Error);
									}

									vMirror.Reset();
									state->voices.Free(vIndex);
									it = state->pendingPlaybacks.erase(it);

									continue;
								}

								AsyncMixerCommand cmd = AsyncMixerCommand::PrepareVoiceStreaming(
									vIndex,
									vGen,
									sIndex,
									sGen,
									sound->channels,
									sound->sampleRate
								);
								state->mixerCommands->Enqueue(cmd);

								frameCount = sound->frameCount;
								channels = sound->channels;
								sampleRate = sound->sampleRate;
							}

							vMirror.pendingLoad = false;
							vMirror.frameCount = frameCount;
							vMirror.channels = channels;
							vMirror.sampleRate = sampleRate;

							// Send voice commands based on current vMirror state
							if (vMirror.state == VoiceState::Playing) {
								state->mixerCommands->Enqueue(AsyncMixerCommand::PlayVoice(vIndex, vGen));
							}
							else if (vMirror.state == VoiceState::Paused) {
								state->mixerCommands->Enqueue(AsyncMixerCommand::PauseVoice(vIndex, vGen));
							}
							else if (vMirror.state == VoiceState::Stopped) {
								state->mixerCommands->Enqueue(AsyncMixerCommand::StopVoice(vIndex, vGen));
							}
						}

						it = state->pendingPlaybacks.erase(it);
					}
					else {
						it++;
					}
				}
				break;
			}
			case AsyncLoadEvent::Type::SoundLoadFailed: {
				for (auto it = state->pendingPlaybacks.begin(); it != state->pendingPlaybacks.end(); ) {
					if (it->assetRawId == ev.assetRawId) {
						state->ModifyVoice(it->playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
							DALIA_LOG_ERR(LOG_CTX_CORE, "Aborting deferred playback. Sound failed to load.");

							if (vMirror.onStopCallback) {
								vMirror.onStopCallback(it->playback, PlaybackExitCondition::Error);
							}

							state->mixerCommands->Enqueue(AsyncMixerCommand::FreeVoice(vIndex,
								EngineInternalState::GetPlaybackGeneration(it->playback)));
							vMirror.Reset();
							state->voices.Free(vIndex);

							return Result::Ok;
						});

						it = state->pendingPlaybacks.erase(it);
					}
					else {
						it++;
					}
				}
				break;
			}
			default:
				break;
		}
		}
	}

	void AsyncDirector::CatchControlEvents(EngineInternalState* state) {
		AsyncControlEvent ev;
		while (m_controlEventQueue.Pop(ev)) {
			switch (ev.type) {
				case AsyncControlEvent::Type::OutputDeviceSwapped:
					state->deviceCoordinator.ProcessOutputDeviceSwappedEvent(ev);
					break;
				default:
					break;
			}
		}
	}
}
