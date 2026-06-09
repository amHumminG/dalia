#include "dalia/audio/Engine.h"
#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "dalia/audio/EffectControl.h"

#include "backend/WasapiBackend.h"

#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"
#include "core/Constants.h"
#include "core/Types.h"
#include "core/Math.h"
#include "core/HandleManager.h"

#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoStreamRequestQueue.h"
#include "messaging/IoLoadRequestQueue.h"
#include "messaging/IoLoadEventQueue.h"

#include "core/StateSyncPool.h"
#include "core/AsyncPool.h"
#include "core/ParameterBridge.h"
#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"
#include "mixer/effects/Biquad.h"
#include "mixer/MixGraphCompiler.h"
#include "mixer/Listener.h"
#include "mixer/RtSystem.h"
#include "mixer/Speakers.h"

#include "io/IoStreamSystem.h"
#include "io/IoLoadSystem.h"

#include "resources/AssetRegistry.h"
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"

#include "common/StringID.h"

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

namespace dalia {

	struct VoiceID {
		uint32_t index;
		uint32_t gen;

		bool operator==(const VoiceID& other) const {
			return index == other.index && gen == other.gen;
		}
	};

	struct PendingSoundUnload {
		SoundHandle handle;
		std::vector<VoiceID> voicesToStop;
	};

	struct PendingPlayback {
		uint32_t voiceIndex = NO_INDEX;
		uint32_t voiceGen = NO_GENERATION;
		uint64_t assetRawId = INVALID_RAW_ID;
	};

	struct EffectRouting {
		uint32_t busIndex = NO_INDEX;
		uint32_t effectSlot = NO_INDEX;
		EffectState effectState = EffectState::None;
	};

	struct EngineInternalState {
		bool initialized = false;

		// Output Settings
		std::unique_ptr<IAudioBackend> backend; // HAL

		SpeakerLayout speakerLayout;
		uint32_t maxSamplesPerPeriod = 0;
		uint32_t outChannels = 0;
		uint32_t outSampleRate = 0;

		CoordinateSystem coordinateSystem;

		// Messaging Queues
		std::unique_ptr<RtCommandQueue>			rtCommands;
		std::unique_ptr<RtEventQueue>			rtEvents;
		std::unique_ptr<IoStreamRequestQueue>	ioStreamRequests;
		std::unique_ptr<IoLoadRequestQueue>		ioLoadRequests;
		std::unique_ptr<IoLoadEventQueue>		ioLoadEvents;

		// Resource Capacities
		uint32_t streamCapacity		= 0;
		uint32_t voiceCapacity		= 0;
		uint32_t listenerCapacity	= 0;
		uint32_t busCapacity		= 0;

		uint32_t biquadCapacity		= 0;

		// Streams
		AsyncPool<StreamContext> streams;

		// Voices
		StateSyncPool<Voice, VoiceMirror, VoiceParams> voices;

		// Listeners
		StateSyncPool<Listener, ListenerMirror, ListenerParams> listeners;

		// Buses
		StateSyncPool<Bus, BusMirror, BusParams> buses;
		std::unordered_map<BusID, uint32_t>		busMap;
#if DALIA_DEBUG
		std::unordered_map<BusID, std::string>	busDebugNameMap; // Unused
#endif
		std::unique_ptr<float[]>				busBufferPool;

		// Effects
		std::unordered_map<uint64_t, EffectRouting> effectRoutingTable; // Maps effect handles to bus routing

		StateSyncPool<Biquad, BiquadMirror, BiquadParams> biquads;

		// Mixing & DSP
		std::unique_ptr<MixGraphCompiler> mixGraphCompiler;
		std::unique_ptr<uint32_t[]> mixOrder;

		std::unique_ptr<float[]> dspScratchBuffer;

		// Assets
		std::unique_ptr<AssetRegistry> assetRegistry;
		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> loadCallbacks;

		// Deferred Unloads & Playbacks
		std::vector<PendingSoundUnload> pendingSoundUnloads;
		std::vector<PendingPlayback> pendingPlaybacks;

		// Systems
		std::unique_ptr<RtSystem> rtSystem;
		std::unique_ptr<IoStreamSystem> ioStreamSystem;
		std::unique_ptr<IoLoadSystem> ioLoadSystem;

		EngineInternalState(const EngineConfig& config)
			: coordinateSystem(config.coordinateSystem),
			streamCapacity(config.streamCapacity),
			voiceCapacity(config.voiceCapacity),
			listenerCapacity(std::clamp(config.listenerCapacity, LISTENERS_MIN, LISTENERS_MAX)),
			busCapacity(config.busCapacity),
			biquadCapacity(config.BiquadCapacity),
			streams(config.streamCapacity),
			voices(config.voiceCapacity),
			listeners(std::clamp(config.listenerCapacity, LISTENERS_MIN, LISTENERS_MAX)),
			buses(config.busCapacity),
			biquads(config.BiquadCapacity) {
			// Message Queues
			rtCommands			= std::make_unique<RtCommandQueue>(config.rtCommandQueueCapacity);
			rtEvents			= std::make_unique<RtEventQueue>(config.rtEventQueueCapacity);
			ioStreamRequests	= std::make_unique<IoStreamRequestQueue>(config.ioStreamRequestQueueCapacity);
			ioLoadRequests		= std::make_unique<IoLoadRequestQueue>(config.ioLoadRequestQueueCapacity);
			ioLoadEvents		= std::make_unique<IoLoadEventQueue>(config.ioLoadEventQueueCapacity);

			// Mixing
			mixGraphCompiler	= std::make_unique<MixGraphCompiler>(config.busCapacity);
			mixOrder			= std::make_unique<uint32_t[]>(config.busCapacity);

			// Resources
			assetRegistry	= std::make_unique<AssetRegistry>(config.residentSoundCapacity, config.streamSoundCapacity);
		}

		uint32_t GenerateIoLoadRequestId() {return nextIoLoadRequestId++; }

		PlaybackHandle ForgePlaybackHandle(uint32_t index, uint32_t generation) {
			return PlaybackHandle::Create(index, generation);
		}

		SoundHandle ForgeSoundHandle(uint64_t rawId) { return SoundHandle::FromRawId(rawId); }

		EffectHandle ForgeEffectHandle(uint64_t rawId) { return EffectHandle::FromRawId(rawId); }
		uint32_t GetEffectIndex(EffectHandle effect) const { return effect.GetIndex(); }
		uint32_t GetEffectGen(EffectHandle effect) const { return effect.GetGeneration(); }

		Result ResolveMirror(uint32_t index, uint32_t gen, VoiceMirror*& mirror) {
			if (index >= voiceCapacity) return Result::InvalidHandle;

			VoiceMirror& vMirror = voices.GetMirror(index);
			if (vMirror.gen != gen) return Result::ExpiredHandle;

			mirror = &vMirror;
			return Result::Ok;
		}

		Result ResolveMirror(uint32_t index, ListenerMirror*& mirror) {
			if (index >= listenerCapacity) return Result::ListenerNotFound;

			mirror = &listeners.GetMirror(index);
			return Result::Ok;
		}

		Result ResolveMirror(const char* busIdentifier, BusMirror*& mirror) {
			const BusID bId(busIdentifier);
			auto it = busMap.find(bId);
			if (it == busMap.end()) return Result::BusNotFound;

			mirror = &buses.GetMirror(it->second);
			return Result::Ok;
		}

		Result ResolveIndex(const char* busIdentifier, uint32_t& index) {
			const BusID bId(busIdentifier);
			auto it = busMap.find(bId);
			if (it == busMap.end()) return Result::BusNotFound;

			index = it->second;
			return Result::Ok;
		}

		Result Validate(PlaybackHandle handle) {
			VoiceMirror* dummy = nullptr;
			return ResolveMirror(handle.GetIndex(), handle.GetGeneration(), dummy);
		}

		Result Validate(EffectHandle handle) {
			uint32_t eIndex = handle.GetIndex();
			uint32_t eGen = handle.GetGeneration();

			switch (handle.GetType()) {
				case EffectType::Biquad:
					if (eIndex >= biquadCapacity || biquads.GetMirror(eIndex).gen != eGen) {
						return Result::InvalidHandle;
					}
					return Result::Ok;

				default:
					return Result::InvalidHandle;
			}
		}

		Result Validate(const char* busIdentifier) {
			BusMirror* dummy = nullptr;
			return ResolveMirror(busIdentifier, dummy);
		}

	};

	// --- INTERNAL HELPERS ---
	static inline bool IsInitialized(const EngineInternalState* state) {
		return state != nullptr && state->initialized;
	}

	static inline Result DispatchStreamPrepare(EngineInternalState* state, const char* filepath, uint32_t& streamIndex,
		uint32_t& streamGen) {
		if (!state->streams.Allocate(streamIndex)) return Result::StreamPoolExhausted;
		streamGen = state->streams.Get(streamIndex).gen.load(std::memory_order_acquire); // Read the current generation

		// Send I/O request to prepare stream
		state->streams.Get(streamIndex).state.store(StreamState::Preparing, std::memory_order_release);
		IoStreamRequest req = IoStreamRequest::PrepareStream(streamIndex,state->streams.Get(streamIndex).gen ,filepath);
		if (!state->ioStreamRequests->Push(req)) {
			// Rollback
			state->streams.Get(streamIndex).state.store(StreamState::Free, std::memory_order_release);
			state->streams.Free(streamIndex);

			return Result::IoStreamRequestQueueFull;
		}

		return Result::Ok;
	}

	static void ProcessRtEvent(EngineInternalState* state,  RtEvent& ev) {
		switch (ev.type) {
			case RtEvent::Type::VoiceStopped: {
				uint32_t vIndex = ev.data.voice.index;
				uint32_t vGen = ev.data.voice.generation;
				VoiceMirror* vMirror;
				Result res = state->ResolveMirror(vIndex, vGen, vMirror);

				if (res == Result::Ok) { // Voice is still valid
					PlaybackExitCallback callback = vMirror->onStopCallback;
					vMirror->Reset();
					state->voices.Free(vIndex);

					DALIA_LOG_DEBUG(LOG_CTX_CORE, "Freed voice %d.", vIndex);
					if (callback) callback(state->ForgePlaybackHandle(vIndex, vGen), ev.data.voice.exitCondition);

					// --- Check garbage collection ---
					VoiceID stoppedVoice = {vIndex, vGen};
					for (auto it = state->pendingSoundUnloads.begin(); it != state->pendingSoundUnloads.end(); ) {
						auto& waitingList = it->voicesToStop;

						for (size_t i = 0; i < waitingList.size(); i++) {
							if (waitingList[i] == stoppedVoice) {
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
			case RtEvent::Type::EffectActive: {
				auto it = state->effectRoutingTable.find(ev.data.effect.handleRawId);
				if (it != state->effectRoutingTable.end()) {
					EffectRouting& routing = it->second;
					routing.effectState = EffectState::Active;
				}
				break;
			}
			case RtEvent::Type::EffectDetached: {
				auto it = state->effectRoutingTable.find(ev.data.effect.handleRawId);
				if (it != state->effectRoutingTable.end()) {
					EffectRouting routing = it->second;

					auto& mirroredHandle = state->buses.GetMirror(routing.busIndex).effectSlots[routing.effectSlot];
					if (mirroredHandle.GetRawId() == ev.data.effect.handleRawId) {
						mirroredHandle = InvalidEffectHandle; // Detach the handle if it's still in the same slot
					}

					state->effectRoutingTable.erase(it);
				}
				break;
			}
		}
	}

	static void ProcessIoLoadEvent(EngineInternalState* state, const IoLoadEvent& ev) {
		// Execute user-registered callback
		auto it = state->loadCallbacks.find(ev.requestId);
		if (it != state->loadCallbacks.end()) {
			if (it->second) {
				it->second(ev.requestId, ev.result); // Call it
			}
			state->loadCallbacks.erase(it);
		}

		switch (ev.type) {
			case IoLoadEvent::Type::SoundLoaded: {
				// Process deferred playbacks
				for (auto it = state->pendingPlaybacks.begin(); it != state->pendingPlaybacks.end(); ) {
					if (it->assetRawId == ev.assetRawId) {
						uint32_t vIndex = it->voiceIndex, vGen = it->voiceGen;
						VoiceMirror* vMirror = nullptr;
						Result res = state->ResolveMirror(vIndex, vGen, vMirror);

						if (res == Result::Ok && vMirror->pendingLoad) {
							SoundHandle handle = state->ForgeSoundHandle(ev.assetRawId);
							SoundType soundType = handle.GetType();
							uint32_t frameCount = 0;
							uint32_t channels = 0;
							uint32_t sampleRate = 0;

							if (soundType == SoundType::Resident) {
								ResidentSound* sound = state->assetRegistry->GetResidentSound(handle);

								RtCommand cmd = RtCommand::PrepareVoiceResident(
									vIndex,
									vGen,
									sound->pcmData.data(),
									sound->frameCount,
									sound->channels,
									sound->sampleRate
								);
								state->rtCommands->Enqueue(cmd);

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

									if (vMirror->onStopCallback) {
										PlaybackHandle playback = state->ForgePlaybackHandle(
											vIndex,
											vGen
										);
										vMirror->onStopCallback(playback, PlaybackExitCondition::Error);
									}

									vMirror->Reset();
									state->voices.Free(vIndex);
									it = state->pendingPlaybacks.erase(it);

									continue;
								}

								RtCommand cmd = RtCommand::PrepareVoiceStreaming(
									vIndex,
									vGen,
									sIndex,
									sGen,
									sound->channels,
									sound->sampleRate
								);
								state->rtCommands->Enqueue(cmd);

								frameCount = sound->frameCount;
								channels = sound->channels;
								sampleRate = sound->sampleRate;
							}

							vMirror->pendingLoad = false;
							vMirror->frameCount = frameCount;
							vMirror->channels = channels;
							vMirror->sampleRate = sampleRate;

							// Send voice commands based on current vMirror state
							if (vMirror->state == VoiceState::Playing) {
								state->rtCommands->Enqueue(RtCommand::PlayVoice(vIndex, vGen));
							}
							else if (vMirror->state == VoiceState::Paused) {
								state->rtCommands->Enqueue(RtCommand::PauseVoice(vIndex, vGen));
							}
							else if (vMirror->state == VoiceState::Stopped) {
								state->rtCommands->Enqueue(RtCommand::StopVoice(vIndex, vGen));
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
			case IoLoadEvent::Type::SoundLoadFailed: {
				for (auto it = state->pendingPlaybacks.begin(); it != state->pendingPlaybacks.end(); ) {
					if (it->assetRawId == ev.assetRawId) {
						uint32_t vIndex = it->voiceIndex, vGen = it->voiceGen;
						VoiceMirror* vMirror = nullptr;
						Result res = state->ResolveMirror(vIndex, vGen, vMirror);

						if (res == Result::Ok) {
							DALIA_LOG_ERR(LOG_CTX_CORE, "Aborting deferred playback. Sound failed to load.");

							if (vMirror->onStopCallback) {
								PlaybackHandle playback = state->ForgePlaybackHandle(
									vIndex,
									vGen
								);
								vMirror->onStopCallback(playback, PlaybackExitCondition::Error);
							}

							state->rtCommands->Enqueue(RtCommand::FreeVoice(vIndex, vGen));
							vMirror->Reset();
							state->voices.Free(vIndex);
						}

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

	inline math::Vector3 FromPublic(const dalia::Vec3& v) {
		return { v.x, v.y, v.z };
	}

	inline Vec3 ToPublic(const math::Vector3& v) {
		return { v.x, v.y, v.z };
	}

	// ------------------------

	Engine::Engine() = default;
	Engine::~Engine() { TeardownInternal(); };

	Result Engine::Init(const EngineConfig& config) {
		if (m_state != nullptr) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to initialize engine that is already initialized.");
			return Result::AlreadyInitialized;
		}

		Logger::Init(config.logLevel, 256);
		if (config.logCallback != nullptr) Logger::SetSink(config.logCallback);

		// --- INTERNAL STATE SETUP ---
		m_state = new EngineInternalState(config);

		// --- Master Bus Setup ---
		uint32_t masterIndex;
		m_state->buses.Allocate(masterIndex);
		m_state->buses.GetMirror(MASTER_BUS_INDEX).refCount = 1;
		m_state->buses.Get(MASTER_BUS_INDEX).isActive = true;

		constexpr BusID masterId("Master");
		m_state->busMap[masterId] = MASTER_BUS_INDEX;
#if DALIA_DEBUG
		m_state->busDebugNameMap[masterId] = "Master";
#endif

		// --- Listener 0 Setup ---
		m_state->listeners.Get(0).params.isActive = true;
		m_state->listeners.GetMirror(0).params.isActive = true;

		// --- BACKEND (HAL) SETUP ---
#ifdef _WIN32
		m_state->backend = std::make_unique<WasapiBackend>();
#else
		DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Unsupported OS.");
		TeardownInternal();
		return Result::SystemError;
#endif

		Result res = m_state->backend->Init();
		if (res != Result::Ok) {
			DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Backend initialization failed.");
			TeardownInternal();
			return res;
		}

		m_state->speakerLayout		= m_state->backend->GetSpeakerLayout();
		if (m_state->speakerLayout == SpeakerLayout::Mono) DALIA_LOG_DEBUG(LOG_CTX_API, "Device is has mono layout.");
		if (m_state->speakerLayout == SpeakerLayout::Stereo) DALIA_LOG_DEBUG(LOG_CTX_API, "Device is has stereo layout.");
		if (m_state->speakerLayout == SpeakerLayout::Surround51) DALIA_LOG_DEBUG(LOG_CTX_API, "Device is has 5.1 layout.");
		if (m_state->speakerLayout == SpeakerLayout::Surround71) DALIA_LOG_DEBUG(LOG_CTX_API, "Device is has 7.1 layout.");

		if (m_state->speakerLayout != SpeakerLayout::Mono && m_state->speakerLayout != SpeakerLayout::Stereo) {
			// Temporary fix until we support more output layouts
			DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Device uses unsupported speaker layout.");
			TeardownInternal();
			return Result::DeviceFailed;
		}

		m_state->outChannels		= m_state->backend->GetChannelCount();
		m_state->outSampleRate		= m_state->backend->GetSampleRate();
		uint32_t bufferSizeInFrames = m_state->backend->GetBufferCapacityInFrames();

		DALIA_LOG_DEBUG(LOG_CTX_API, "Backend period size: %d.", m_state->backend->GetPeriodSizeInFrames());
		DALIA_LOG_DEBUG(LOG_CTX_API, "Backend buffer size: %d.", bufferSizeInFrames);

		// Buffer allocations based on period size
		m_state->maxSamplesPerPeriod = bufferSizeInFrames * CHANNELS_MAX;
		uint32_t busBufferPoolSize = m_state->busCapacity * m_state->maxSamplesPerPeriod;
		m_state->busBufferPool = std::make_unique<float[]>(busBufferPoolSize);
		m_state->dspScratchBuffer = std::make_unique<float[]>(m_state->maxSamplesPerPeriod);

		// --- SYSTEMS SETUP ---
		RtSystemConfig rtConfig;
		rtConfig.coordinateSystem		= m_state->coordinateSystem;
		rtConfig.speakerLayout			= m_state->speakerLayout;
		rtConfig.maxSamplesPerPeriod	= m_state->maxSamplesPerPeriod;
		rtConfig.outChannels			= m_state->outChannels;
		rtConfig.outSampleRate			= m_state->outSampleRate;
		rtConfig.rtCommands				= m_state->rtCommands.get();
		rtConfig.rtEvents				= m_state->rtEvents.get();
		rtConfig.ioStreamRequests		= m_state->ioStreamRequests.get();
		rtConfig.streamPool				= m_state->streams.GetSpan();
		rtConfig.listenerPool			= m_state->listeners.GetSpan();
		rtConfig.listenerParamBridges	= m_state->listeners.GetParamBridgeSpan();
		rtConfig.voicePool				= m_state->voices.GetSpan();
		rtConfig.voiceParamBridges		= m_state->voices.GetParamBridgeSpan();
		rtConfig.busPool				= m_state->buses.GetSpan();
		rtConfig.busParamBridges		= m_state->buses.GetParamBridgeSpan();
		rtConfig.busBufferPool			= std::span(m_state->busBufferPool.get(), busBufferPoolSize);
		rtConfig.biquadPool				= m_state->biquads.GetSpan();
		rtConfig.biquadParamBridges		= m_state->biquads.GetParamBridgeSpan();
		rtConfig.mixGraphCompiler		= m_state->mixGraphCompiler.get();
		rtConfig.mixOrder				= std::span(m_state->mixOrder.get(), m_state->busCapacity);
		rtConfig.dspScratchBuffer		= std::span(m_state->dspScratchBuffer.get(), m_state->maxSamplesPerPeriod);
		m_state->rtSystem = std::make_unique<RtSystem>(rtConfig);

		m_state->backend->AttachSystem(m_state->rtSystem.get()); // Attach audio system to backend

		IoStreamSystemConfig ioStreamingConfig;
		ioStreamingConfig.outSampleRate		= m_state->outSampleRate;
		ioStreamingConfig.ioStreamRequests	= m_state->ioStreamRequests.get();
		ioStreamingConfig.streamPool		= m_state->streams.GetSpan();
		ioStreamingConfig.freeStreams		= m_state->streams.GetFreeList();
		m_state->ioStreamSystem	= std::make_unique<IoStreamSystem>(ioStreamingConfig);

		IoLoadSystemConfig ioLoadSystemConfig;
		ioLoadSystemConfig.outSampleRate	= m_state->outSampleRate;
		ioLoadSystemConfig.ioLoadRequests	= m_state->ioLoadRequests.get();
		ioLoadSystemConfig.ioLoadEvents		= m_state->ioLoadEvents.get();
		ioLoadSystemConfig.assetRegistry	= m_state->assetRegistry.get();
		m_state->ioLoadSystem = std::make_unique<IoLoadSystem>(ioLoadSystemConfig);

		// --- SYSTEMS START ---
		// I/O Thread Start
		m_state->ioStreamSystem->Start();
		m_state->ioLoadSystem->Start();

		// Audio Thread Start
		res = m_state->backend->Start();
		if (res != Result::Ok) {
			DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Failed to start audio thread.");
			TeardownInternal();
			return res;
		}

		m_state->initialized = true;
		DALIA_LOG_INFO(LOG_CTX_API, "Initialized engine.");
		return Result::Ok;
	}

	Result Engine::Shutdown() {
		if (!IsInitialized(m_state)) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to shutdown uninitialized engine.");
			return Result::NotInitialized;
		}

		DALIA_LOG_INFO(LOG_CTX_API, "Engine shutdown.");
		TeardownInternal();

		return Result::Ok;
	}

	void Engine::Update() {
		if (!IsInitialized(m_state)) return;

		// --- Process Event Inbox ---
		RtEvent RtEv;
		while (m_state->rtEvents->Pop(RtEv)) ProcessRtEvent(m_state, RtEv);

		IoLoadEvent loadEv;
		while (m_state->ioLoadEvents->Pop(loadEv)) ProcessIoLoadEvent(m_state, loadEv);

		// --- Update Parameter Bridges ---
		for (uint32_t vIndex = 0; vIndex < m_state->voiceCapacity; vIndex++) {
			VoiceMirror& vMirror = m_state->voices.GetMirror(vIndex);
			if (vMirror.state == VoiceState::Free || !vMirror.isParamsDirty) continue;

			m_state->voices.GetParamBridge(vIndex).PushUpdate(vMirror.params);
			vMirror.isParamsDirty = false;
		}

		for (uint32_t lIndex = 0; lIndex < m_state->listenerCapacity; lIndex++) {
			ListenerMirror& lMirror = m_state->listeners.GetMirror(lIndex);
			if (!lMirror.isParamsDirty) continue;

			m_state->listeners.GetParamBridge(lIndex).PushUpdate(lMirror.params);
			lMirror.isParamsDirty = false;
		}

		for (uint32_t bIndex = 0; bIndex < m_state->busCapacity; bIndex++) {
			BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
			if (!bMirror.isParamsDirty) continue;

			m_state->buses.GetParamBridge(bIndex).PushUpdate(bMirror.params);
			bMirror.isParamsDirty = false;
		}

		for (uint32_t eIndex = 0; eIndex < m_state->biquadCapacity; eIndex++) {
			BiquadMirror& eMirror = m_state->biquads.GetMirror(eIndex);
			if (!eMirror.isParamsDirty) continue;

			m_state->biquads.GetParamBridge(eIndex).PushUpdate(eMirror.params);
			eMirror.isParamsDirty = false;
		}

		m_state->rtCommands->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}

	Result Engine::SetGlobalDopplerFactor(float globalDopplerFactor) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		float clampedGlobalDopplerFactor = std::clamp(globalDopplerFactor, DOPPLER_FACTOR_MIN, DOPPLER_FACTOR_MAX);
		m_state->rtCommands->Enqueue(RtCommand::SetGlobalDopplerFactor(clampedGlobalDopplerFactor));

		return Result::Ok;
	}

	Result Engine::LoadSoundAsync(SoundHandle& sound, SoundType soundType, const char* filepath, AssetLoadCallback callback,
	                              uint32_t* outRequestId) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		const SoundID pathId(filepath);

		// Duplicate check
		if (m_state->assetRegistry->GetLoadedSoundHandle(pathId, sound)) {
			if (sound.GetType() == soundType) {
				if (soundType == SoundType::Resident) {
					ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
					if (residentSound) residentSound->refCount++;
				}
				else if (soundType == SoundType::Stream) {
					StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
					if (streamSound) streamSound->refCount++;
				}

				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (outRequestId) *outRequestId = INVALID_REQUEST_ID;

				return Result::Ok;
			}
		}

		sound = m_state->assetRegistry->AllocateSound(soundType);
		if (!sound.IsValid()) {
			if (soundType == SoundType::Resident) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to load sound %s. Resident sound pool exhausted.", filepath);
			}
			else if (soundType == SoundType::Stream) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to load sound %s. Stream sound pool exhausted.", filepath);
			}
			return Result::StreamSoundPoolExhausted;
		}

		if (soundType == SoundType::Resident) {
			ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
			residentSound->refCount = 1;
			residentSound->pathHash = pathId.GetHash();
			residentSound->state.store(LoadState::Loading, std::memory_order_relaxed);
		}
		else if (soundType == SoundType::Stream) {
			StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
			streamSound->refCount = 1;
			streamSound->pathHash = pathId.GetHash();
			streamSound->state.store(LoadState::Loading, std::memory_order_relaxed);
		}
		m_state->assetRegistry->RegisterLoadedSound(pathId, sound);

		uint32_t requestId = m_state->GenerateIoLoadRequestId();
		if (outRequestId) *outRequestId = requestId;
		if (callback) m_state->loadCallbacks[requestId] = std::move(callback);

		m_state->ioLoadRequests->Push(IoLoadRequest::LoadSound(requestId, sound, filepath));

		return Result::Ok;
	}

	Result Engine::UnloadSound(SoundHandle sound) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = sound.GetType();
		uint32_t soundRefCount;
		uint32_t soundPathHash;

		if (soundType == SoundType::Resident) {
			ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
			if (!residentSound) return Result::InvalidHandle;
			if (residentSound->refCount > 0) residentSound->refCount--;
			soundRefCount = residentSound->refCount;
			soundPathHash = residentSound->pathHash;
		}
		else {
			StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
			if (!streamSound) return Result::InvalidHandle;
			if (streamSound->refCount > 0) streamSound->refCount--;
			soundRefCount = streamSound->refCount;
			soundPathHash = streamSound->pathHash;
		}

		if (soundRefCount == 0) {
			m_state->assetRegistry->UnregisterLoadedSound(SoundID::FromHash(soundPathHash));

			PendingSoundUnload pendingUnload;
			pendingUnload.handle = sound;

			// Remove pending playbacks for the sound
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->assetRawId == sound.GetRawId()) {
					VoiceMirror& vMirror = m_state->voices.GetMirror(it->voiceIndex);

					if (vMirror.onStopCallback) {
						PlaybackHandle playback = PlaybackHandle::Create(it->voiceIndex, it->voiceGen);
						vMirror.onStopCallback(playback, PlaybackExitCondition::ExplicitStop);
					}

					vMirror.Reset();
					m_state->voices.Free(it->voiceIndex);

					DALIA_LOG_DEBUG(LOG_CTX_API, "Aborted deferred playback for voice %d due to early unload.",
						it->voiceIndex);
					it = m_state->pendingPlaybacks.erase(it);
				}
				else it++;
			}

			// Command active voices using the asset to stop
			for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
				VoiceMirror& vMirror = m_state->voices.GetMirror(i);

				if (vMirror.state != VoiceState::Free && vMirror.assetRawId == sound.GetRawId()) {
					pendingUnload.voicesToStop.push_back(VoiceID(i, vMirror.gen));

					m_state->rtCommands->Enqueue(RtCommand::StopVoice(i, vMirror.gen));
					DALIA_LOG_DEBUG(LOG_CTX_API, "Commanded to stop voice %d.", i);
				}
			}

			if (!pendingUnload.voicesToStop.empty()) {
				m_state->pendingSoundUnloads.push_back(std::move(pendingUnload));
			}
			else {
				m_state->assetRegistry->FreeSound(sound);
				DALIA_LOG_DEBUG(LOG_CTX_API, "Unloaded sound with handle %d.", sound.GetRawId());
			}
		}

		return Result::Ok;
	}

	Result Engine::GetSoundLength(SoundHandle sound, double& lengthInSeconds) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = sound.GetType();

		if (soundType == SoundType::Resident) {
			ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
			if (!residentSound) return Result::InvalidHandle;
			lengthInSeconds = residentSound->lengthInSeconds;
		}
		else {
			StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
			if (!streamSound) return Result::InvalidHandle;
			lengthInSeconds = streamSound->lengthInSeconds;
		}

		return Result::Ok;
	}

	Result Engine::CreateBus(const char* identifier, const char* parentIdentifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t bIndex;
		uint32_t bIndexParent = NO_PARENT;
		bool parentFound = false;

		// Fetch parent bus
		Result resParent = m_state->ResolveIndex(parentIdentifier, bIndexParent);
		if (resParent == Result::Ok) parentFound = true;

		// Fetch bus
		Result res = m_state->ResolveIndex(identifier, bIndex);
		if (res == Result::Ok) {
			BusMirror& bMirror  = m_state->buses.GetMirror(bIndex);
			bMirror.refCount++;

			if (parentFound && bMirror.parentBusIndex != bIndexParent) {
				DALIA_LOG_WARN(LOG_CTX_API,
					"CreateBus called for existing bus (%s) with conflicting parent. Ignoring new parent.", identifier);
			}
			return Result::Ok;
		}

		if (!parentFound) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create bus %s (routed -> %s). No bus with identifier %s exists.",
				identifier, parentIdentifier, parentIdentifier);
			return resParent;
		}

		// Bus does not exist yet
		if (!m_state->buses.Allocate(bIndex)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create bus (%s). Bus pool exhausted.", identifier);
			return Result::BusPoolExhausted;
		}

		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		bMirror.refCount = 1;
		bMirror.parentBusIndex = bIndexParent;

		const BusID bId(identifier);
		m_state->busMap[bId] = bIndex;
#if DALIA_DEBUG
		m_state->busDebugNameMap[bId] = identifier;
#endif

		DALIA_LOG_DEBUG(LOG_CTX_API, "Created bus with routing: %s (index: %d) -> %s (index: %d).",
			identifier, bIndex, parentIdentifier, bIndexParent);
		m_state->rtCommands->Enqueue(RtCommand::AllocateBus(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result Engine::DestroyBus(const char* identifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		auto it = m_state->busMap.find(BusID(identifier));
		if (it == m_state->busMap.end()) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy bus (%s). Bus does not exist.", identifier);
			return Result::BusNotFound;
		}
		uint32_t bIndex = it->second;

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy bus (%s). Master cannot be destroyed.", identifier);
			return Result::Error;
		}

		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		if (bMirror.refCount > 1) {
			bMirror.refCount--;
			return Result::Ok;
		}

		// Reference count is 1 -> Destroy bus

		// Remove parent from child buses
		uint32_t orphanedBuses = 0;
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			BusMirror* bMirrorChild = &m_state->buses.GetMirror(i);
			if (bMirrorChild->parentBusIndex == bIndex) {
				bMirrorChild->parentBusIndex = NO_PARENT;
				m_state->rtCommands->Enqueue(RtCommand::SetBusParent(i, NO_PARENT));
				orphanedBuses++;
				DALIA_LOG_DEBUG(LOG_CTX_API, "Orphaned bus (index: %d).", i);
			}
		}
		if (orphanedBuses > 0) DALIA_LOG_WARN(LOG_CTX_API, "Destroying bus %s. Orphaned %d buses.",
			identifier, orphanedBuses);

		uint32_t orphanedPlaybacks = 0;
		for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
			VoiceMirror& vMirrorChild = m_state->voices.GetMirror(i);
			if (vMirrorChild.parentBusIndex == bIndex) {
				vMirrorChild.parentBusIndex = NO_PARENT;
				m_state->rtCommands->Enqueue(RtCommand::SetVoiceParent(i, vMirrorChild.gen, NO_PARENT));
				orphanedPlaybacks++;
				DALIA_LOG_DEBUG(LOG_CTX_API, "Orphaned voice (index: %d).", i);
			}
		}
		if (orphanedBuses > 0) DALIA_LOG_WARN(LOG_CTX_API, "Destroying bus %s. Orphaned %d playbacks.",
			identifier, orphanedPlaybacks);

		bMirror.Reset();
		m_state->busMap.erase(it);
#if DALIA_DEBUG
		m_state->busDebugNameMap.erase(BusID(identifier));
#endif

		m_state->buses.Free(bIndex);

		DALIA_LOG_DEBUG(LOG_CTX_API, "Destroyed bus %s (index: %d).", identifier, bIndex);
		m_state->rtCommands->Enqueue(RtCommand::FreeBus(bIndex));

		return Result::Ok;
	}

	Result Engine::RouteBus(const char* identifier, const char* parentIdentifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		// Fetch bus
		uint32_t bIndex;
		Result res = m_state->ResolveIndex(identifier, bIndex);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). No bus with identifier %s exists.",
				identifier, parentIdentifier, identifier);
			return res;
		}

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). Master cannot be routed.",
				identifier, parentIdentifier);
			return Result::InvalidRouting;
		}

		// Fetch parent bus
		uint32_t bIndexParent;
		Result resParent = m_state->ResolveIndex(parentIdentifier, bIndexParent);
		if (resParent != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). No bus with identifier %s exists.",
				identifier, parentIdentifier, parentIdentifier);
			return resParent;
		}

		// Cycle detection
		uint32_t currentAncestor = bIndexParent;
		while (currentAncestor != NO_PARENT) {
			if (currentAncestor == bIndex) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). Bus graph cycle detected.",
					identifier, parentIdentifier);
				return Result::InvalidRouting;
			}
			currentAncestor = m_state->buses.GetMirror(currentAncestor).parentBusIndex;
		}

		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		if (bMirror.parentBusIndex == bIndexParent) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to route bus (%s -> %s). %s is already routed to %s.",
				identifier, parentIdentifier, identifier, parentIdentifier);
			return Result::Ok;
		}
		bMirror.parentBusIndex = bIndexParent;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Routed bus %s (index: %d) -> %s (index: %d).",
			identifier, bIndex, parentIdentifier, bIndexParent);
		m_state->rtCommands->Enqueue(RtCommand::SetBusParent(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result Engine::SetBusVolumeDb(const char* identifier, float volumeDb) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		// Fetch bus
		BusMirror* bMirror;
		Result res = m_state->ResolveMirror(identifier, bMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set bus volume. No bus with identifier (%s) exists", identifier);
			return res;
		}

		float clampedVolumeDb = std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX);
		bMirror->params.gain = math::DbToGain(clampedVolumeDb);
		bMirror->isParamsDirty = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set bus (%s) volume to %.2f dB.", identifier, clampedVolumeDb);

		return Result::Ok;
	}

	template <typename TParams>
	requires requires(TParams p) {p.Sanitize(); }
	Result Engine::CreateEffect(EffectHandle& effect, const TParams& params) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t eIndex = 0;
		uint32_t eGen = NO_GENERATION;

		TParams sanitizedParams = params;
		sanitizedParams.Sanitize();

		if constexpr (std::is_same_v<TParams, BiquadParams>) {
			if (!m_state->biquads.Allocate(eIndex)) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed effect (biquad). Capacity reached.");
				return Result::EffectPoolExhausted;
			}

			BiquadMirror& mirror = m_state->biquads.GetMirror(eIndex);
			eGen = mirror.gen;

			effect = EffectHandle::Create(eIndex, eGen, EffectType::Biquad);
			mirror.params = sanitizedParams;
			mirror.isParamsDirty = true;
		}
		else {
			static_assert(sizeof(TParams) == 0, "Unsupported effect parameters passed to CreateEffect.");
		}

		m_state->rtCommands->Enqueue(RtCommand::AllocateEffect(EffectType::Biquad, eIndex, eGen));

		DALIA_LOG_DEBUG(LOG_CTX_API, "Created effect (index: %u, gen: %u).", eIndex, eGen);
		return Result::Ok;
	}

	template <typename TParams>
	requires requires(TParams p) {p.Sanitize(); }
	Result Engine::SetEffectParams(EffectHandle effect, const TParams& params) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t eIndex = effect.GetIndex();
		uint32_t eGen = effect.GetGeneration();
		EffectType eType = effect.GetType();

		TParams sanitizedParams = params;
		sanitizedParams.Sanitize();

		if constexpr (std::is_same_v<TParams, BiquadParams>) {
			if (eType != EffectType::Biquad || eIndex >= m_state->biquadCapacity) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to set effect params. Invalid effect handle.");
				return Result::InvalidHandle;
			}

			BiquadMirror& eMirror = m_state->biquads.GetMirror(eIndex);
			if (eMirror.gen != eGen) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to set effect params. Expired effect handle.");
				return Result::ExpiredHandle;
			}

			eMirror.params = sanitizedParams;
			eMirror.isParamsDirty = true;
		}
		else {
			static_assert(sizeof(TParams) == 0, "Unsupported effect parameters passed to SetEffectParams.");
		}

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set effect params for handle with rawId: 0x%016llx.", effect.GetRawId());
		return Result::Ok;
	}

	Result Engine::AttachEffect(EffectHandle effect, const char* busIdentifier, uint32_t effectSlot) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		Result res = m_state->Validate(effect);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %d). Invalid or expired effect handle.",
				busIdentifier, effectSlot);

			return res;
		}

		// Fetch bus
		uint32_t bIndex;
		Result resBus = m_state->ResolveIndex(busIdentifier, bIndex);
		if (resBus != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %d). No bus with identifier %s exists.",
				busIdentifier, effectSlot, busIdentifier);
			return resBus;
		}

		if (effectSlot >= MAX_EFFECTS_PER_BUS) {
			DALIA_LOG_ERR(LOG_CTX_API,
				"Failed to attach effect to bus %s (slot %d). Valid effect slots are in the range [0, %d].",
				busIdentifier, effectSlot, MAX_EFFECTS_PER_BUS-1);
			return Result::InvalidEffectSlot;
		}

		// Steal effect if already attached
		auto it = m_state->effectRoutingTable.find(effect.GetRawId());
		if (it != m_state->effectRoutingTable.end()) {
			EffectRouting routing = it->second;
			BusMirror& bMirror = m_state->buses.GetMirror(routing.busIndex);
			bMirror.effectSlots[routing.effectSlot] = InvalidEffectHandle;

			RtCommand cmd = RtCommand::ForceDetachEffect(
				effect.GetIndex(),
				effect.GetGeneration(),
				effect.GetType(),
				routing.busIndex,
				routing.effectSlot
			);
			m_state->rtCommands->Enqueue(cmd);

			DALIA_LOG_DEBUG(LOG_CTX_API,
				"Attaching effect to %s (index: %d) (slot: %d) but effect is already attached. Stealing effect.",
				busIdentifier, bIndex, effectSlot);

			m_state->effectRoutingTable.erase(it);
		}

		// Check effect slot
		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		if (bMirror.effectSlots[effectSlot].IsValid()) {
			DALIA_LOG_WARN(LOG_CTX_API, "Detaching effect from bus %s (slot %d). Attaching new effect to slot.",
				busIdentifier, effectSlot);

			// Detach old effect
			EffectHandle oldEffect = bMirror.effectSlots[effectSlot];
			m_state->effectRoutingTable.erase(oldEffect.GetRawId());

			RtCommand detachCmd = RtCommand::ForceDetachEffect(
				oldEffect.GetIndex(),
				oldEffect.GetGeneration(),
				oldEffect.GetType(),
				bIndex,
				effectSlot
			);
			m_state->rtCommands->Enqueue(detachCmd);
		}

		bMirror.effectSlots[effectSlot] = effect;
		m_state->effectRoutingTable[effect.GetRawId()] = EffectRouting(bIndex, effectSlot);

		RtCommand cmd = RtCommand::AttachEffect(
			effect.GetIndex(),
			effect.GetGeneration(),
			effect.GetType(),
			bIndex,
			effectSlot
		);
		m_state->rtCommands->Enqueue(cmd);

		DALIA_LOG_DEBUG(LOG_CTX_API, "Attached effect (handle rawId: 0x%016llx) to bus %s (slot %d).",
			effect.GetRawId(), busIdentifier, effectSlot);

		return Result::Ok;
	}

	Result Engine::DetachEffect(EffectHandle effect) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		Result res = m_state->Validate(effect);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy effect. Invalid or expired effect handle");
			return res;
		}

		// Detach effect if attached
		auto it = m_state->effectRoutingTable.find(effect.GetRawId());
		if (it != m_state->effectRoutingTable.end()) {
			EffectRouting& routing = it->second;
			routing.effectState = EffectState::FadingOut;

			RtCommand cmd = RtCommand::FadeDetachEffect(
				effect.GetIndex(),
				effect.GetGeneration(),
				effect.GetType(),
				routing.busIndex,
				routing.effectSlot
			);
			m_state->rtCommands->Enqueue(cmd);

			DALIA_LOG_DEBUG(LOG_CTX_API,
				"Detached effect (rawId: 0x%016llx) from bus (index: %d) (slot: %d).",
				effect.GetRawId(), routing.busIndex, routing.effectSlot);

			return Result::Ok;
		}
		DALIA_LOG_WARN(LOG_CTX_API, "Attempted to detach effect that is not attached.");

		return Result::Ok;
	}

	Result Engine::DestroyEffect(EffectHandle effect) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;


		Result res = m_state->Validate(effect);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy effect. Invalid or expired effect handle");
			return res;
		}

		// Free the effect
		uint32_t eIndex = effect.GetIndex();

		switch (effect.GetType()) {
			case EffectType::Biquad:
				m_state->biquads.GetMirror(eIndex).Reset();
				m_state->biquads.Free(eIndex);
				break;

			default:
				break;
		}
		// Detach if attached
		auto it = m_state->effectRoutingTable.find(effect.GetRawId());
		if (it != m_state->effectRoutingTable.end()) {
			EffectRouting routing = it->second;
			BusMirror& bMirror = m_state->buses.GetMirror(routing.busIndex);
			bMirror.effectSlots[routing.effectSlot] = InvalidEffectHandle;

			RtCommand detachCmd = RtCommand::ForceDetachEffect(
				effect.GetIndex(),
				effect.GetGeneration(),
				effect.GetType(),
				routing.busIndex,
				routing.effectSlot
			);
			m_state->rtCommands->Enqueue(detachCmd);

			DALIA_LOG_DEBUG(LOG_CTX_API, "Detaching effect from bus (index: %d) (slot %d).",
				routing.busIndex, routing.effectSlot);

			m_state->effectRoutingTable.erase(it);
		}

		RtCommand cmd = RtCommand::FreeEffect(effect.GetIndex(), effect.GetGeneration(), effect.GetType());
		m_state->rtCommands->Enqueue(cmd);

		DALIA_LOG_DEBUG(LOG_CTX_API, "Destroyed effect (handle rawId: 0x%016llx).", effect.GetRawId());

		return Result::Ok;
	}

	Result Engine::CreatePlayback(PlaybackHandle& pbkHandle, SoundHandle soundHandle, PlaybackExitCallback callback) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

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
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create playback instance. Voice pool exhausted.");
			return Result::VoicePoolExhausted;
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
				vIndex,
				vMirror.gen,
				soundHandle.GetRawId()
			});
			vMirror.pendingLoad = true;
			DALIA_LOG_DEBUG(LOG_CTX_API, "Deferring playback for voice %d. Sound not yet loaded.", vIndex);

			pbkHandle = PlaybackHandle::Create(vIndex, vMirror.gen);
			m_state->rtCommands->Enqueue(RtCommand::AllocateVoice(vIndex, vMirror.gen));
			return Result::Ok;
		}

		if (soundType == SoundType::Resident) {
			m_state->rtCommands->Enqueue(RtCommand::AllocateVoice(vIndex, vMirror.gen));
			RtCommand cmd = RtCommand::PrepareVoiceResident(
				vIndex,
				vMirror.gen,
				residentSound->pcmData.data(),
				residentSound->frameCount,
				residentSound->channels,
				residentSound->sampleRate
			);
			m_state->rtCommands->Enqueue(cmd);
		}
		else if (soundType == SoundType::Stream) {
			uint32_t sIndex;
			uint32_t sGen;
			Result streamRes = DispatchStreamPrepare(m_state, streamSound->filepath, sIndex, sGen);
			if (streamRes != Result::Ok) {
				vMirror.Reset();
				m_state->voices.Free(vIndex);

				if (streamRes == Result::StreamPoolExhausted) {
					DALIA_LOG_ERR(LOG_CTX_API, "Failed to prepare stream instance. Stream pool exhausted.");
				}
				if (streamRes == Result::IoStreamRequestQueueFull) {
					DALIA_LOG_ERR(LOG_CTX_API, "Failed to prepare stream instance. I/O stream request queue full.");
				}

				return streamRes;
			}

			m_state->rtCommands->Enqueue(RtCommand::AllocateVoice(vIndex, vMirror.gen));
			RtCommand cmd = RtCommand::PrepareVoiceStreaming(
				vIndex,
				vMirror.gen,
				sIndex,
				sGen,
				streamSound->channels,
				streamSound->sampleRate
			);
			m_state->rtCommands->Enqueue(cmd);
		}

		pbkHandle = PlaybackHandle::Create(vIndex, vMirror.gen);
		DALIA_LOG_DEBUG(LOG_CTX_API, "Created playback instance using voice %d.", vIndex);

		return Result::Ok;
	}

	Result Engine::RoutePlayback(PlaybackHandle playback, const char* busIdentifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		// Fetch voice mirror
		VoiceMirror* vMirror = nullptr;
		Result resVoice = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (resVoice != Result::Ok) return resVoice;

		// Fetch bus
		uint32_t bIndex;
		const Result resBus = m_state->ResolveIndex(busIdentifier, bIndex);
		if (resBus != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route playback. No bus with identifier %s exists.", busIdentifier);
			return resBus;
		}

		if (vMirror->parentBusIndex == bIndex) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to route playback to %s. Playback is already routed to %s.",
				busIdentifier, busIdentifier);
			return Result::Ok;
		}
		vMirror->parentBusIndex = bIndex;
		m_state->rtCommands->Enqueue(RtCommand::SetVoiceParent(vIndex, vGen, bIndex));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Routed voice %d to bus %s (index: %d).", vIndex, busIdentifier, bIndex);

		return Result::Ok;
	}

	Result Engine::PlayPlayback(PlaybackHandle handle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = handle.GetIndex();
		uint32_t vGen = handle.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		// Deferred playback
		if (vMirror->pendingLoad) {
			vMirror->state = VoiceState::Playing;
			DALIA_LOG_DEBUG(LOG_CTX_API, "Calling play on for sound that is still loading. Will play when done loading.");
			return Result::Ok;
		}

		// Check for double calls
		if (vMirror->state == VoiceState::Playing) {
			DALIA_LOG_WARN(LOG_CTX_API, "Calling play on playback that is already set to play.");
			return Result::Ok;
		}

		if (vMirror->state != VoiceState::Inactive && vMirror->state != VoiceState::Paused) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to start playback. Playback corrupted.");
			DALIA_LOG_DEBUG(LOG_CTX_API,
				"Failed to play voice (index: %d). Mirror state corrupted (State: %d).",
				vIndex, static_cast<int>(vMirror->state));
			return Result::PlaybackCorrupted;
		}

		vMirror->state = VoiceState::Playing;

		m_state->rtCommands->Enqueue(RtCommand::PlayVoice(vIndex, vGen));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to play.", vIndex);

		return Result::Ok;
	}

	Result Engine::PausePlayback(PlaybackHandle playback) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		if (vMirror->state != VoiceState::Playing) {
			DALIA_LOG_WARN(LOG_CTX_API, "Calling pause on playback that is not currently playing.");
			return Result::Ok;
		}
		vMirror->state = VoiceState::Paused;
		if (vMirror->pendingLoad) return Result::Ok;

		m_state->rtCommands->Enqueue(RtCommand::PauseVoice(vIndex, vGen));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to pause.", vIndex);

		return Result::Ok;
	}

	Result Engine::StopPlayback(PlaybackHandle playback) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		// Handle pending playbacks
		if (vMirror->pendingLoad) {
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->voiceIndex == vIndex) {
					if (vMirror->onStopCallback) {
						vMirror->onStopCallback(playback, PlaybackExitCondition::ExplicitStop);
					}

					vMirror->Reset();
					m_state->voices.Free(vIndex);
					DALIA_LOG_DEBUG(LOG_CTX_API, "Stopped voice %d before it started playing.", vIndex);
					m_state->pendingPlaybacks.erase(it);
					return Result::Ok;
				}
				else it++;
			}
		}

		vMirror->state = VoiceState::Stopped;

		m_state->rtCommands->Enqueue(RtCommand::StopVoice(vIndex, vGen));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to stop.", vIndex);

		return Result::Ok;
	}

	Result Engine::SeekPlayback(PlaybackHandle playback, double timeInSeconds) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		if (timeInSeconds < 0.0) timeInSeconds = 0.0;

		uint64_t seekFrame64 = static_cast<uint64_t>(timeInSeconds * vMirror->sampleRate);

		// Safety clamps
		if (seekFrame64 >= vMirror->frameCount) {
			DALIA_LOG_WARN(LOG_CTX_API, "Seeking beyond final sound frame. Clamping to final frame.");
			if (vMirror->frameCount > 0) seekFrame64 = vMirror->frameCount - 1;
			else seekFrame64 = 0;
		}

		uint32_t seekFrame = static_cast<uint32_t>(seekFrame64);
		m_state->rtCommands->Enqueue(RtCommand::SeekVoice(vIndex, vGen, seekFrame));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Seeking voice %d to frame %d.", vIndex, seekFrame);

		return Result::Ok;
	}

	Result Engine::SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		float clampedVolumeDb = std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX);
		vMirror->params.gain = math::DbToGain(clampedVolumeDb);
		vMirror->isParamsDirty = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u volume to %.2f.", vIndex, clampedVolumeDb);

		return Result::Ok;
	}

	Result Engine::SetPlaybackPitch(PlaybackHandle playback, float pitch) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		float clampedPitch = std::clamp(pitch, PITCH_MIN, PITCH_MAX);
		vMirror->params.pitch = clampedPitch;
		vMirror->isParamsDirty = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u pitch to %.2f.", vIndex, clampedPitch);

		return Result::Ok;
	}

	Result Engine::SetPlaybackStereoPan(PlaybackHandle playback, float pan) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		float clampedStereoPan = std::clamp(pan, PAN_STEREO_MIN, PAN_STEREO_MAX);
		vMirror->params.stereoPan = clampedStereoPan;
		vMirror->isParamsDirty = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u stereo pan to %.2f.", vIndex, clampedStereoPan);

		return Result::Ok;
	}

	Result Engine::SetPlaybackLooping(PlaybackHandle playback, bool looping) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		if (vMirror->params.isLooping == looping) {
			if (looping) DALIA_LOG_WARN(LOG_CTX_API, "Setting playback to loop. Playback is already set to loop.");
			else DALIA_LOG_WARN(LOG_CTX_API, "Setting playback to not loop. Playback is already set to not loop.");
			return Result::Ok;
		}

		vMirror->params.isLooping = looping;
		vMirror->isParamsDirty = true;

		if (looping) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u to loop.", vIndex);
		else DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u to not loop.", vIndex);

		return Result::Ok;
	}

	Result Engine::SetPlaybackSpatial(PlaybackHandle playback, bool spatial) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		if (vMirror->params.isSpatial == spatial) {
			if (spatial) DALIA_LOG_WARN(LOG_CTX_API, "Setting playback to spatial. Playback is already spatial.");
			else DALIA_LOG_WARN(LOG_CTX_API, "Setting playback to non-spatial. Playback is already set to non-spatial.");
			return Result::Ok;
		}

		vMirror->params.isSpatial = spatial;
		vMirror->isParamsDirty = true;

		if (spatial) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u to spatial.", vIndex);
		else DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u to not spatial.", vIndex);

		return Result::Ok;
	}

	Result Engine::SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode mode) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.distanceMode = mode;
		vMirror->isParamsDirty = true;

		if (mode == DistanceMode::FromListener) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u distance mode to FromListener.", vIndex);
		else DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u distance mode to FromProbe.", vIndex);

		return Result::Ok;
	}

	Result Engine::SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.attenuationCurve = curve;
		vMirror->isParamsDirty = true;

		if (curve == AttenuationCurve::InverseSquare) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u attenuation curve to InverseSquare.", vIndex);
		else if (curve == AttenuationCurve::Linear) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u attenuation curve to Linear.", vIndex);
		else if (curve == AttenuationCurve::Quadratic) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u attenuation curve to Quadratic.", vIndex);

		return Result::Ok;
	}

	Result Engine::SetPlaybackPosition(PlaybackHandle playback, const Vec3& position) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.position = FromPublic(position);
		vMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		float clampedMinDistance = std::max(minDistance, MIN_DIST_MIN);
		float clampedMaxDistance = std::max(maxDistance, clampedMinDistance);

		vMirror->params.minDistance = clampedMinDistance;
		vMirror->params.maxDistance = clampedMaxDistance;
		vMirror->isParamsDirty = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u min/max distance to %.2f/%.2f.", vIndex, clampedMinDistance, clampedMaxDistance);

		return Result::Ok;
	}

	Result Engine::SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.useDoppler = useDoppler;
		vMirror->isParamsDirty = true;

		if (useDoppler) DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u to use doppler.", vIndex);
		else DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u to not use doppler.", vIndex);

		return Result::Ok;
	}

	Result Engine::SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		float clampedDopplerFactor = std::clamp(dopplerFactor, DOPPLER_FACTOR_MIN, DOPPLER_FACTOR_MAX);
		vMirror->params.dopplerFactor = clampedDopplerFactor;
		vMirror->isParamsDirty = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set Voice %u doppler factor to %.2f.", vIndex, clampedDopplerFactor);

		return Result::Ok;
	}

	Result Engine::SetPlaybackVelocity(PlaybackHandle playback, const Vec3& velocity) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.velocity = FromPublic(velocity);
		vMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask mask) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = m_state->ResolveMirror(vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.listenerMask = mask; // Should we double-check this somehow?
		vMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetListenerActive(uint32_t listenerIndex, bool active) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		ListenerMirror* lMirror = nullptr;
		Result res = m_state->ResolveMirror(listenerIndex, lMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set listener active/inactive. Listener with index %u does not exist.",
				listenerIndex);
		}

		lMirror->params.isActive = active;
		lMirror->isParamsDirty = true;

		if (active) DALIA_LOG_DEBUG(LOG_CTX_API, "Set listener %u active", listenerIndex);
		else DALIA_LOG_DEBUG(LOG_CTX_API, "Set listener %u inactive", listenerIndex);

		return Result::Ok;
	}

	Result Engine::SetListener3DAttributes(uint32_t listenerIndex, const Listener3DAttributes& attributes) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		ListenerMirror* lMirror = nullptr;
		Result res = m_state->ResolveMirror(listenerIndex, lMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set listener attributes. Listener with index %u does not exist.",
				listenerIndex);
		}

		lMirror->params.position				= FromPublic(attributes.position);
		lMirror->params.distanceProbePosition	= FromPublic(attributes.distanceProbePosition);
		lMirror->params.forward					= math::Vector3::Normalize(FromPublic(attributes.forward));
		lMirror->params.up						= math::Vector3::Normalize(FromPublic(attributes.up));
		lMirror->params.velocity				= FromPublic(attributes.velocity);
		lMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetListenerPosition(uint32_t listenerIndex, const Vec3& position) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		ListenerMirror* lMirror = nullptr;
		Result res = m_state->ResolveMirror(listenerIndex, lMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set listener position. Listener with index %u does not exist.",
				listenerIndex);
		}

		lMirror->params.position = FromPublic(position);
		lMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetListenerDistanceProbePosition(uint32_t listenerIndex, const Vec3& distanceProbePosition) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		ListenerMirror* lMirror = nullptr;
		Result res = m_state->ResolveMirror(listenerIndex, lMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API,
				"Failed to set listener distance probe position. Listener with index %u does not exist.",
				listenerIndex);
		}

		lMirror->params.distanceProbePosition = FromPublic(distanceProbePosition);
		lMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetListenerOrientation(uint32_t listenerIndex, const Vec3& forward, const Vec3& up) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		ListenerMirror* lMirror = nullptr;
		Result res = m_state->ResolveMirror(listenerIndex, lMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set listener orientation. Listener with index %u does not exist.",
				listenerIndex);
		}

		lMirror->params.forward	= math::Vector3::Normalize(FromPublic(forward));
		lMirror->params.up		= math::Vector3::Normalize(FromPublic(up));
		lMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetListenerVelocity(uint32_t listenerIndex, const Vec3& velocity) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		ListenerMirror* lMirror = nullptr;
		Result res = m_state->ResolveMirror(listenerIndex, lMirror);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set listener velocity. Listener with index %u does not exist.",
				listenerIndex);
		}

		lMirror->params.velocity = FromPublic(velocity);
		lMirror->isParamsDirty = true;

		return Result::Ok;
	}

	void Engine::TeardownInternal() {
		if (!m_state) return;

		if (m_state->backend)			m_state->backend->Stop();
		if (m_state->ioLoadSystem)		m_state->ioLoadSystem->Stop();
		if (m_state->ioStreamSystem)	m_state->ioStreamSystem->Stop();

		delete m_state;
		m_state = nullptr;

		Logger::Deinit();
	}

	// --- Template Instantiations ---

	// Biquad Filter
	template Result Engine::CreateEffect<BiquadParams>(EffectHandle&, const BiquadParams&);
	template Result Engine::SetEffectParams<BiquadParams>(EffectHandle, const BiquadParams&);
}
