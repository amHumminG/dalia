#include "MessageRouter.h"

#include "engine/EngineInternalState.h"
#include "resource/AssetRegistry.h"

#include "resource/StreamSound.h"
#include "resource/ResidentSound.h"

namespace dalia {
	MessageRouter::MessageRouter(size_t mixerQueueCapacity, size_t streamQueueCapacity, size_t loadQueueCapacity,
		size_t controlQueueCapacity)
			: m_mixerCommandQueue(mixerQueueCapacity),
			m_streamCommandQueue(streamQueueCapacity),
			m_loadCommandQueue(loadQueueCapacity),
			m_controlCommandQueue(controlQueueCapacity),
			m_mixerEventQueue(mixerQueueCapacity),
			m_loadEventQueue(loadQueueCapacity),
			m_controlEventQueue(controlQueueCapacity) {
	}

	void MessageRouter::DispatchStagedCommands() {
		m_mixerCommandQueue.Dispatch();
	}

	void MessageRouter::ProcessMainEvents() {
		CatchMixerEvents();
		CatchLoadEvents();
		CatchControlEvents();
	}

	void MessageRouter::ProcessMixerCommands() {
		CatchMixerCommands();
	}

	void MessageRouter::ProcessStreamCommands() {
		CatchStreamCommands();
	}

	void MessageRouter::ProcessLoadCommands() {
		CatchLoadCommands();
	}

	void MessageRouter::ProcessControlCommands() {
		CatchControlCommands();
	}

	// void MessageRouter::SetStreamWakeupCallback(WakeupCallback cb) {
	// 	m_streamWakeup = std::move(cb);
	// }

	void MessageRouter::SetLoadWakeupCallback(WakeupCallback cb) {
		m_loadWakeup = std::move(cb);
	}

	void MessageRouter::SetControlWakeupCallback(WakeupCallback cb) {
		m_controlWakeup = std::move(cb);
	}

	void MessageRouter::PushMixerCommand(const AsyncMixerCommand& command) {
		m_mixerCommandQueue.Enqueue(command);
	}

	bool MessageRouter::PushStreamCommand(const AsyncStreamCommand& command) {
		if (m_streamCommandQueue.Push(command)) {
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push stream command. Queue is at capacity.");
		return false;
	}

	bool MessageRouter::PushLoadCommand(const AsyncLoadCommand& command) {
		bool wasEmtpy = false;
		if (m_loadCommandQueue.Push(command, wasEmtpy)) {
			if (wasEmtpy && m_loadWakeup) m_loadWakeup();
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push load command. Queue is at capacity.");
		return false;
	}

	bool MessageRouter::PushControlCommand(const AsyncControlCommand& command) {
		bool wasEmpty = false;
		if (m_controlCommandQueue.Push(command, wasEmpty)) {
			if (wasEmpty && m_controlWakeup) m_controlWakeup();
			return true;
		}
		DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push control command. Queue is at capacity.");
		return false;
	}

	void MessageRouter::SubscribeToMixerCommand(AsyncMixerCommand::Type type,
		MessageCallback<AsyncMixerCommand> cb) {
		m_mixerCommandTable.Subscribe(type, cb);
	}

	void MessageRouter::SubscribeToStreamCommand(AsyncStreamCommand::Type type,
	MessageCallback<AsyncStreamCommand> cb) {
		m_streamCommandTable.Subscribe(type, cb);
	}

	void MessageRouter::SubscribeToLoadCommand(AsyncLoadCommand::Type type,
		MessageCallback<AsyncLoadCommand> cb) {
		m_loadCommandTable.Subscribe(type, cb);
	}

	void MessageRouter::SubscribeToControlCommand(AsyncControlCommand::Type type,
		MessageCallback<AsyncControlCommand> cb) {
		m_controlCommandTable.Subscribe(type, cb);
	}

	void MessageRouter::UnsubscribeFromMixerCommand(AsyncMixerCommand::Type type) {
		m_mixerCommandTable.Unsubscribe(type);
	}

	void MessageRouter::UnsubscribeFromStreamCommand(AsyncStreamCommand::Type type) {
		m_streamCommandTable.Unsubscribe(type);
	}

	void MessageRouter::UnsubscribeFromLoadCommand(AsyncLoadCommand::Type type) {
		m_loadCommandTable.Unsubscribe(type);
	}

	void MessageRouter::UnsubscribeFromControlCommand(AsyncControlCommand::Type type) {
		m_controlCommandTable.Unsubscribe(type);
	}

	bool MessageRouter::PushMixerEvent(const AsyncMixerEvent& event) {
		if (!m_mixerEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push mixer event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	bool MessageRouter::PushLoadEvent(const AsyncLoadEvent& event) {
		if (!m_loadEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push load event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	bool MessageRouter::PushControlEvent(const AsyncControlEvent& event) {
		if (!m_controlEventQueue.Push(event)) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to push control event. Queue is at capacity.");
			return false;
		}
		return true;
	}

	void MessageRouter::SubscribeToMixerEvent(AsyncMixerEvent::Type type, MessageCallback<AsyncMixerEvent> cb) {
		m_mixerEventTable.Subscribe(type, cb);
	}

	void MessageRouter::SubscribeToLoadEvent(AsyncLoadEvent::Type type, MessageCallback<AsyncLoadEvent> cb) {
		m_loadEventTable.Subscribe(type, cb);
	}

	void MessageRouter::SubscribeToControlEvent(AsyncControlEvent::Type type, MessageCallback<AsyncControlEvent> cb) {
		m_controlEventTable.Subscribe(type, cb);
	}

	void MessageRouter::UnsubscribeFromMixerEvent(AsyncMixerEvent::Type type) {
		m_mixerEventTable.Unsubscribe(type);
	}

	void MessageRouter::UnsubscribeFromLoadEvent(AsyncLoadEvent::Type type) {
		m_loadEventTable.Unsubscribe(type);
	}

	void MessageRouter::UnsubscribeFromControlEvent(AsyncControlEvent::Type type) {
		m_controlEventTable.Unsubscribe(type);
	}

	void MessageRouter::CatchMixerCommands() {
		AsyncMixerCommand cmd;
		while (m_mixerCommandQueue.Pop(cmd)) m_mixerCommandTable.Dispatch(cmd);
	}

	void MessageRouter::CatchStreamCommands() {
		AsyncStreamCommand cmd;
		while (m_streamCommandQueue.Pop(cmd)) m_streamCommandTable.Dispatch(cmd);
	}

	void MessageRouter::CatchLoadCommands() {
		AsyncLoadCommand cmd;
		while (m_loadCommandQueue.Pop(cmd)) m_loadCommandTable.Dispatch(cmd);
	}

	void MessageRouter::CatchControlCommands() {
		AsyncControlCommand cmd;
		while (m_controlCommandQueue.Pop(cmd)) m_controlCommandTable.Dispatch(cmd);
	}

	void MessageRouter::CatchMixerEvents() {
		AsyncMixerEvent ev;
		while (m_mixerEventQueue.Pop(ev)) m_mixerEventTable.Dispatch(ev);
	}

	void MessageRouter::CatchLoadEvents() {
		AsyncLoadEvent ev;
		while (m_loadEventQueue.Pop(ev)) m_loadEventTable.Dispatch(ev);
	}

	void MessageRouter::CatchControlEvents() {
		AsyncControlEvent ev;
		while (m_controlEventQueue.Pop(ev)) m_controlEventTable.Dispatch(ev);
	}

	// TODO: Move this logic
	void MessageRouter::CatchMixerEvents(EngineInternalState& state) {
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

	// TODO: Move this logic
	void MessageRouter::CatchLoadEvents(EngineInternalState* state) {
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
}
