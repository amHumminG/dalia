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

#include "mixer/ParameterBridge.h"
#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"
#include "mixer/MixGraphCompiler.h"
#include "mixer/effects/BiquadFilter.h"
#include "mixer/Listener.h"
#include "mixer/RtSystem.h"
#include "mixer/Speakers.h"

#include "io/IoStreamSystem.h"
#include "io/IoLoadSystem.h"

#include "resources/AssetRegistry.h"
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"

#include "common/StringID.h"

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
		uint64_t assetUuid = INVALID_UUID;
	};

	struct EffectRouting {
		uint32_t busIndex = NO_INDEX;
		uint32_t effectSlot = NO_INDEX;
		EffectState effectState = EffectState::None;
	};

	struct EngineInternalState {
		bool initialized = false;

		// --- Output Thingies ---
		std::unique_ptr<IAudioBackend> backend; // HAL

		uint32_t outChannels = 0;
		uint32_t outSampleRate = 0;

		SpeakerLayout speakerLayout;

		// --- Messaging Queues ---
		std::unique_ptr<RtCommandQueue>			rtCommands;
		std::unique_ptr<RtEventQueue>			rtEvents;
		std::unique_ptr<IoStreamRequestQueue>	ioStreamRequests;
		std::unique_ptr<IoLoadRequestQueue>		ioLoadRequests;
		std::unique_ptr<IoLoadEventQueue>		ioLoadEvents;

		// --- Resource Capacities ---
		uint32_t voiceCapacity	= 0;
		uint32_t streamCapacity	= 0;
		uint32_t busCapacity	= 0;

		uint32_t listenerCapacity = 0;

		// --- Pools ---
		std::unique_ptr<Voice[]>				voicePool;
		std::unique_ptr<VoiceMirror[]>			voicePoolMirror;
		std::unique_ptr<ParameterBridge<VoiceParams>[]>	voiceParamBridges;

		std::unique_ptr<StreamContext[]>		streamPool;

		std::unique_ptr<Bus[]>					busPool;
		std::unique_ptr<float[]>				busBufferPool;
		std::unique_ptr<BusMirror[]>			busPoolMirror;
		std::unordered_map<BusID, uint32_t>		busHashMap;
#if DALIA_DEBUG
		std::unordered_map<BusID, std::string>	busDebugNames; // Not currently used
#endif

		std::unique_ptr<Listener[]>				listenerPool;
		std::unique_ptr<ListenerMirror[]>		listenerPoolMirror;
		std::unique_ptr<ParameterBridge<ListenerParams>[]>	listenerParamBridges;

		// --- Availability Containers ---
		std::unique_ptr<FixedStack<uint32_t>>		freeVoices;
		std::unique_ptr<SPSCRingBuffer<uint32_t>>	freeStreams;
		std::unique_ptr<FixedStack<uint32_t>>		freeBuses;

		// --- Effects ---
		std::unique_ptr<float[]> dspScratchBuffer;

		std::unordered_map<uint64_t, EffectRouting> effectRoutingTable; // Maps effect handles to bus routing

		std::unique_ptr<BiquadFilter[]> biquadFilterPool;
		std::unique_ptr<HandleManager> biquadHM;

		std::unique_ptr<MixGraphCompiler> mixGraphCompiler;
		std::unique_ptr<uint32_t[]> mixOrder;

		// --- Resources ---
		std::unique_ptr<AssetRegistry> assetRegistry;
		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> loadCallbacks;

		// Deferred Unloads
		std::vector<PendingSoundUnload> pendingSoundUnloads;

		// Deferred playback
		std::vector<PendingPlayback> pendingPlaybacks;

		std::unique_ptr<RtSystem> rtSystem;
		std::unique_ptr<IoStreamSystem> ioStreamSystem;
		std::unique_ptr<IoLoadSystem> ioLoadSystem;

		EngineInternalState(const EngineConfig& config)
			: voiceCapacity(config.voiceCapacity), streamCapacity(config.streamCapacity), busCapacity(config.busCapacity),
			listenerCapacity(std::clamp(config.listenerCapacity, LISTENERS_MIN, LISTENERS_MAX)) {
			// Message Queues
			rtCommands			= std::make_unique<RtCommandQueue>(config.rtCommandQueueCapacity);
			rtEvents			= std::make_unique<RtEventQueue>(config.rtEventQueueCapacity);
			ioStreamRequests	= std::make_unique<IoStreamRequestQueue>(config.ioStreamRequestQueueCapacity);
			ioLoadRequests		= std::make_unique<IoLoadRequestQueue>(config.ioLoadRequestQueueCapacity);
			ioLoadEvents		= std::make_unique<IoLoadEventQueue>(config.ioLoadEventQueueCapacity);

			// Pools
			voicePool			= std::make_unique<Voice[]>(voiceCapacity);
			voicePoolMirror		= std::make_unique<VoiceMirror[]>(voiceCapacity);
			voiceParamBridges	= std::make_unique<ParameterBridge<VoiceParams>[]>(voiceCapacity);
			streamPool			= std::make_unique<StreamContext[]>(streamCapacity);
			busPool				= std::make_unique<Bus[]>(busCapacity);
			busPoolMirror		= std::make_unique<BusMirror[]>(busCapacity);

			listenerPool		= std::make_unique<Listener[]>(listenerCapacity);
			listenerPoolMirror  = std::make_unique<ListenerMirror[]>(listenerCapacity);
			listenerParamBridges = std::make_unique<ParameterBridge<ListenerParams>[]>(listenerCapacity);

			// Effects
			biquadFilterPool	= std::make_unique<BiquadFilter[]>(config.biquadCapacity);

			biquadHM			= std::make_unique<HandleManager>(config.biquadCapacity);

			mixGraphCompiler	= std::make_unique<MixGraphCompiler>(config.busCapacity);
			mixOrder			= std::make_unique<uint32_t[]>(config.busCapacity);

			// Resources
			assetRegistry	= std::make_unique<AssetRegistry>(config.residentSoundCapacity, config.streamSoundCapacity);

			// Availability Containers
			freeVoices	= std::make_unique<FixedStack<uint32_t>>(voiceCapacity);
			freeStreams = std::make_unique<SPSCRingBuffer<uint32_t>>(streamCapacity);
			freeBuses	= std::make_unique<FixedStack<uint32_t>>(busCapacity);
			for (int i = voiceCapacity - 1; i >= 0; i--)	freeVoices->Push(i);
			for (int i = 0; i < streamCapacity; i++)	freeStreams->Push(i);     // Queue, dont push in reverse
			for (int i = busCapacity - 1; i >= 1; i--)		freeBuses->Push(i);   // Skip Master (index 0)
		}

		uint32_t GenerateIoLoadRequestId() {return nextIoLoadRequestId++; }

		PlaybackHandle ForgePlaybackHandle(uint32_t index, uint32_t generation) {
			return PlaybackHandle::Create(index, generation);
		}

		SoundHandle ForgeSoundHandle(uint64_t uuid) { return SoundHandle::FromUUID(uuid); }
		EffectHandle ForgeEffectHandle(uint64_t uuid) { return EffectHandle::FromUUID(uuid); }
	};

	// --- INTERNAL HELPERS ---
	static inline bool IsInitialized(const EngineInternalState* state) {
		return state != nullptr && state->initialized;
	}

	static inline Result ResolveVoiceMirror(EngineInternalState* state, uint32_t index, uint32_t generation, VoiceMirror*& outMirror) {
		if (index >= state->voiceCapacity) return Result::InvalidHandle;

		VoiceMirror* mirror = &state->voicePoolMirror[index];
		if (mirror->gen != generation) return Result::ExpiredHandle;

		outMirror = mirror;
		return Result::Ok;
	}

	static inline Result ResolveBusIndex(EngineInternalState* state, const char* identifier, uint32_t& outIndex) {
		const BusID bId(identifier);

		auto it = state->busHashMap.find(bId);
		if (it == state->busHashMap.end()) return Result::BusNotFound;

		outIndex = it->second;
		return Result::Ok;
	}

	static inline Result DispatchStreamPrepare(EngineInternalState* state, const char* filepath, uint32_t& streamIndex) {
		if (!state->freeStreams->Pop(streamIndex)) return Result::StreamPoolExhausted;

		// Send I/O request to prepare stream
		state->streamPool[streamIndex].state.store(StreamState::Preparing, std::memory_order_release);
		IoStreamRequest req = IoStreamRequest::PrepareStream(streamIndex,state->streamPool[streamIndex].gen ,filepath);
		if (!state->ioStreamRequests->Push(req)) {
			// Rollback
			state->streamPool[streamIndex].state.store(StreamState::Free, std::memory_order_release);
			state->freeStreams->Push(streamIndex);

			return Result::IoStreamRequestQueueFull;
		}

		return Result::Ok;
	}

	static inline Result ValidateEffectHandle(const EngineInternalState* state, EffectHandle effect) {
		if (!effect.IsValid()) return Result::InvalidHandle;

		const uint32_t index = effect.GetIndex();
		const uint32_t gen = effect.GetGeneration();

		switch (effect.GetType()) {
			case EffectType::None: {
				return Result::InvalidHandle;
			}
			case EffectType::Biquad: {
				if (!state->biquadHM->IsValid(index, gen)) return Result::ExpiredHandle;
				break;
			}
		default:
			return Result::InvalidHandle;
		}

		return Result::Ok;
	}

	static inline Result FreeEffectHandle(const EngineInternalState* state, EffectHandle effect) {
		if (!effect.IsValid()) return Result::InvalidHandle;

		const uint32_t index = effect.GetIndex();
		const uint32_t gen = effect.GetGeneration();

		switch (effect.GetType()) {
		case EffectType::None: {
			return Result::InvalidHandle;
		}
		case EffectType::Biquad: {
			if (!state->biquadHM->Free(index, gen)) return Result::ExpiredHandle;
			break;
		}
		default:
			return Result::InvalidHandle;
		}

		return Result::Ok;
	}

	static void ProcessRtEvent(EngineInternalState* state,  RtEvent& ev) {
		switch (ev.type) {
			case RtEvent::Type::VoiceStopped: {
				uint32_t index = ev.data.voice.index;
				uint32_t generation = ev.data.voice.generation;
				VoiceMirror* vMirror;
				Result res = ResolveVoiceMirror(state, index, generation, vMirror);

				if (res == Result::Ok) { // Voice is still valid
					AudioEventCallback callback = vMirror->onStopCallback;
					vMirror->Reset();
					state->freeVoices->Push(index);

					DALIA_LOG_DEBUG(LOG_CTX_CORE, "Freed voice %d.", index);
					if (callback) callback(state->ForgePlaybackHandle(index, generation), ev.data.voice.exitCondition);

					// --- Check garbage collection ---
					VoiceID stoppedVoice = {index, generation};
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
								typeStr, it->handle.GetUUID());

							it = state->pendingSoundUnloads.erase(it);
						}
						else it++;
					}
				}
				break;
			}
			case RtEvent::Type::EffectActive: {
				auto it = state->effectRoutingTable.find(ev.data.effect.handleUUID);
				if (it != state->effectRoutingTable.end()) {
					EffectRouting& routing = it->second;
					routing.effectState = EffectState::Active;
				}
				break;
			}
			case RtEvent::Type::EffectDetached: {
				auto it = state->effectRoutingTable.find(ev.data.effect.handleUUID);
				if (it != state->effectRoutingTable.end()) {
					EffectRouting routing = it->second;
					auto& mirroredHandle = state->busPoolMirror[routing.busIndex].effectSlots[routing.effectSlot];
					if (mirroredHandle.GetUUID() == ev.data.effect.handleUUID) {
						// Detach the handle if it's still in the same slot
						mirroredHandle = InvalidEffectHandle;
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
					if (it->assetUuid == ev.assetUuid) {
						uint32_t vIndex = it->voiceIndex, vGen = it->voiceGen;
						VoiceMirror* vMirror = nullptr;
						Result res = ResolveVoiceMirror(state, vIndex, vGen, vMirror);

						if (res == Result::Ok && vMirror->pendingLoad) {
							SoundHandle handle = state->ForgeSoundHandle(ev.assetUuid);
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

								uint32_t streamIndex;
								Result streamResult = DispatchStreamPrepare(state, sound->filepath, streamIndex);
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
									state->freeVoices->Push(vIndex);
									it = state->pendingPlaybacks.erase(it);

									continue;
								}

								RtCommand cmd = RtCommand::PrepareVoiceStreaming(
									vIndex,
									vGen,
									streamIndex,
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
					if (it->assetUuid == ev.assetUuid) {
						uint32_t vIndex = it->voiceIndex, vGen = it->voiceGen;
						VoiceMirror* vMirror = nullptr;
						Result res = ResolveVoiceMirror(state, vIndex, vGen, vMirror);

						if (res == Result::Ok) {
							DALIA_LOG_ERR(LOG_CTX_CORE, "Aborting deferred playback. Sound failed to load.");

							if (vMirror->onStopCallback) {
								PlaybackHandle playback = state->ForgePlaybackHandle(
									vIndex,
									vGen
								);
								vMirror->onStopCallback(playback, PlaybackExitCondition::Error);
							}

							state->rtCommands->Enqueue(RtCommand::DeallocateVoice(vIndex, vGen));
							vMirror->Reset();
							state->freeVoices->Push(vIndex);
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

	// static inline void ResolveAndFlushGains(EngineInternalState* state, uint32_t vIndex) {
	// 	VoiceMirror& vMirror = state->voicePoolMirror[vIndex];
	//
	// 	float volumeLinear = math::DbToGain(vMirror.volumeDb);
	// 	float finalGainMatrix[CHANNELS_MAX][CHANNELS_MAX] = {0};
	//
	// 	if (vMirror.isSpatial) {
	// 		// 3D spatial
	// 		// Calculate distance attenuation & VBAP
	// 	}
	// 	else {
	// 		if (vMirror.channels == CHANNELS_MONO) {
	// 			// Constant power panning
	// 			float panNormalized = (vMirror.stereoPan + 1.0f) * 0.5f;
	// 			float angle = panNormalized * PI * 0.5f;
	//
	// 			finalGainMatrix[0][0] = std::cos(angle) * volumeLinear; // Left
	// 			finalGainMatrix[0][1] = std::sin(angle) * volumeLinear; // Right
	//
	// 			DALIA_LOG_DEBUG(LOG_CTX_CORE,
	// 				"Updated gain matrix for voice %d (MONO). Gain[0][0] = %.2f, Gain[0][1] = %.2f.",
	// 				vIndex, finalGainMatrix[0][0], finalGainMatrix[1][1]);
	// 		}
	// 		else { // CHANNELS_STEREO
	// 			float gainL = std::clamp(1.0f - vMirror.stereoPan, 0.0f, 1.0f);
	// 			float gainR = std::clamp(1.0f + vMirror.stereoPan, 0.0f, 1.0f);
	//
	// 			finalGainMatrix[0][0] = gainL * volumeLinear;
	// 			finalGainMatrix[1][1] = gainR * volumeLinear;
	//
	// 			DALIA_LOG_DEBUG(LOG_CTX_CORE,
	// 				"Updated gain matrix for voice %d (STEREO). Gain[0][0] = %.2f, Gain[1][1] = %.2f.",
	// 				vIndex, finalGainMatrix[0][0], finalGainMatrix[1][1]);
	// 		}
	// 	}
	//
	// 	state->rtCommands->Enqueue(RtCommand::SetVoiceGainMatrix(vIndex, vMirror.gen, finalGainMatrix));
	// 	vMirror.isGainDirty = false;
	// }

	// ------------------------

	Engine::Engine() = default;
	Engine::~Engine() { TeardownInternal(); };

	Result Engine::Init(const EngineConfig& config) {
		if (m_state != nullptr) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to initialize engine that is already initialized.");
			return Result::AlreadyInitialized;
		}

		Logger::Init(config.logLevel, 256);

		// --- INTERNAL STATE SETUP ---
		m_state = new EngineInternalState(config);

		// --- Master Bus Setup ---
		m_state->busPoolMirror[MASTER_BUS_INDEX].refCount = 1;

		constexpr BusID masterId("Master");
		m_state->busHashMap[masterId] = MASTER_BUS_INDEX;
#if DALIA_DEBUG
		m_state->busDebugNames[masterId] = "Master";
#endif

		Bus& master = m_state->busPool[MASTER_BUS_INDEX];
		master.isActive = true;

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
		if (m_state->speakerLayout != SpeakerLayout::Mono || m_state->speakerLayout != SpeakerLayout::Stereo) {
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
		const uint32_t maxSamplesPerPeriod = bufferSizeInFrames * CHANNELS_MAX;
		uint32_t busBufferPoolSize = m_state->busCapacity * maxSamplesPerPeriod;
		m_state->busBufferPool = std::make_unique<float[]>(busBufferPoolSize);
		m_state->dspScratchBuffer = std::make_unique<float[]>(maxSamplesPerPeriod);

		// --- SYSTEMS SETUP ---
		RtSystemConfig rtConfig;
		rtConfig.speakerLayout		= m_state->speakerLayout;
		rtConfig.outChannels		= m_state->outChannels;
		rtConfig.outSampleRate		= m_state->outSampleRate;
		rtConfig.rtCommands			= m_state->rtCommands.get();
		rtConfig.rtEvents			= m_state->rtEvents.get();
		rtConfig.ioStreamRequests	= m_state->ioStreamRequests.get();
		rtConfig.voicePool			= std::span(m_state->voicePool.get(), m_state->voiceCapacity);
		rtConfig.voiceParamBridges  = std::span(m_state->voiceParamBridges.get(), m_state->voiceCapacity);
		rtConfig.streamPool			= std::span(m_state->streamPool.get(), m_state->streamCapacity);
		rtConfig.busPool			= std::span(m_state->busPool.get(), m_state->busCapacity);
		rtConfig.busBufferPool		= std::span(m_state->busBufferPool.get(), busBufferPoolSize);
		rtConfig.mixGraphCompiler	= m_state->mixGraphCompiler.get();
		rtConfig.mixOrder			= std::span(m_state->mixOrder.get(), m_state->busCapacity);
		rtConfig.listenerPool		= std::span(m_state->listenerPool.get(), m_state->listenerCapacity);
		rtConfig.listenerParamBridges = std::span(m_state->listenerParamBridges.get(), m_state->listenerCapacity);
		rtConfig.dspScratchBuffer	= std::span(m_state->dspScratchBuffer.get(), maxSamplesPerPeriod);
		rtConfig.biquadFilterPool	= std::span(m_state->biquadFilterPool.get(), m_state->biquadHM->GetCapacity());
		m_state->rtSystem = std::make_unique<RtSystem>(rtConfig);

		m_state->backend->AttachSystem(m_state->rtSystem.get()); // Hand it to backend for ticking

		IoStreamSystemConfig ioStreamingConfig;
		ioStreamingConfig.outSampleRate		= m_state->outSampleRate;
		ioStreamingConfig.ioStreamRequests	= m_state->ioStreamRequests.get();
		ioStreamingConfig.streamPool		= std::span(m_state->streamPool.get(), m_state->streamCapacity);
		ioStreamingConfig.freeStreams		= m_state->freeStreams.get();
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
		while (m_state->rtEvents->Pop(RtEv)) {
			ProcessRtEvent(m_state, RtEv);
		}

		IoLoadEvent loadEv;
		while (m_state->ioLoadEvents->Pop(loadEv)) {
			ProcessIoLoadEvent(m_state, loadEv);
		}

		// Update voice params if necessary
		for (uint32_t vIndex = 0; vIndex < m_state->voiceCapacity; vIndex++) {
			VoiceMirror& vMirror = m_state->voicePoolMirror[vIndex];
			if (vMirror.state == VoiceState::Free || !vMirror.isParamsDirty) continue;

			m_state->voiceParamBridges[vIndex].PushUpdate(vMirror.params);
			vMirror.isParamsDirty = false;
		}

		m_state->rtCommands->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
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

	Result Engine::UnloadSound(SoundHandle soundHandle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = soundHandle.GetType();
		uint32_t soundRefCount;
		uint32_t soundPathHash;

		if (soundType == SoundType::Resident) {
			ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
			if (!sound) return Result::InvalidHandle;
			if (sound->refCount > 0) sound->refCount--;
			soundRefCount = sound->refCount;
			soundPathHash = sound->pathHash;
		}
		else {
			StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
			if (!sound) return Result::InvalidHandle;
			if (sound->refCount > 0) sound->refCount--;
			soundRefCount = sound->refCount;
			soundPathHash = sound->pathHash;
		}

		if (soundRefCount == 0) {
			m_state->assetRegistry->UnregisterLoadedSound(SoundID::FromHash(soundPathHash));

			PendingSoundUnload pendingUnload;
			pendingUnload.handle = soundHandle;

			// Remove pending playbacks for the sound
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->assetUuid == soundHandle.GetUUID()) {
					VoiceMirror& vMirror = m_state->voicePoolMirror[it->voiceIndex];

					if (vMirror.onStopCallback) {
						PlaybackHandle playback = PlaybackHandle::Create(it->voiceIndex, it->voiceGen);
						vMirror.onStopCallback(playback, PlaybackExitCondition::ExplicitStop);
					}

					vMirror.Reset();
					m_state->freeVoices->Push(it->voiceIndex);

					DALIA_LOG_DEBUG(LOG_CTX_API, "Aborted deferred playback for voice %d due to early unload.",
						it->voiceIndex);
					it = m_state->pendingPlaybacks.erase(it);
				}
				else it++;
			}

			// Command active voices using the asset to stop
			for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
				VoiceMirror& vMirror = m_state->voicePoolMirror[i];

				if (vMirror.state != VoiceState::Free && vMirror.assetUuid == soundHandle.GetUUID()) {
					pendingUnload.voicesToStop.push_back(VoiceID(i, vMirror.gen));

					m_state->rtCommands->Enqueue(RtCommand::StopVoice(i, vMirror.gen));
					DALIA_LOG_DEBUG(LOG_CTX_API, "Commanded to stop voice %d.", i);
				}
			}

			if (!pendingUnload.voicesToStop.empty()) {
				m_state->pendingSoundUnloads.push_back(std::move(pendingUnload));
			}
			else {
				m_state->assetRegistry->FreeSound(soundHandle);
				DALIA_LOG_DEBUG(LOG_CTX_API, "Unloaded sound with handle %d.", soundHandle.GetUUID());
			}
		}

		return Result::Ok;
	}

	Result Engine::CreateBus(const char* identifier, const char* parentIdentifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t bIndex;
		uint32_t bIndexParent = NO_PARENT;
		bool parentFound = false;

		// Fetch parent bus
		Result resParent = ResolveBusIndex(m_state, parentIdentifier, bIndexParent);
		if (resParent == Result::Ok) parentFound = true;

		// Fetch bus
		Result res = ResolveBusIndex(m_state, identifier, bIndex);
		if (res == Result::Ok) {
			BusMirror& bMirror  = m_state->busPoolMirror[bIndex];
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
		if (!m_state->freeBuses->Pop(bIndex)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create bus (%s). Bus pool exhausted.", identifier);
			return Result::BusPoolExhausted;
		}

		BusMirror& bMirror = m_state->busPoolMirror[bIndex];
		bMirror.refCount = 1;
		bMirror.parentBusIndex = bIndexParent;

		const BusID bId(identifier);
		m_state->busHashMap[bId] = bIndex;
#if DALIA_DEBUG
		m_state->busDebugNames[bId] = identifier;
#endif

		DALIA_LOG_DEBUG(LOG_CTX_API, "Created bus with routing: %s (index: %d) -> %s (index: %d).",
			identifier, bIndex, parentIdentifier, bIndexParent);
		m_state->rtCommands->Enqueue(RtCommand::AllocateBus(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result Engine::DestroyBus(const char* identifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		auto it = m_state->busHashMap.find(BusID(identifier));
		if (it == m_state->busHashMap.end()) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy bus (%s). Bus does not exist.", identifier);
			return Result::BusNotFound;
		}
		uint32_t bIndex = it->second;

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy bus (%s). Master cannot be destroyed.", identifier);
			return Result::Error;
		}

		BusMirror& bMirror = m_state->busPoolMirror[bIndex];

		if (bMirror.refCount > 1) {
			bMirror.refCount--;
			return Result::Ok;
		}

		// Reference count is 1 -> Destroy bus

		// Remove parent from child buses
		uint32_t orphanedBuses = 0;
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			BusMirror* bMirrorChild = &m_state->busPoolMirror[i];
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
			VoiceMirror& vMirrorChild = m_state->voicePoolMirror[i];
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
		m_state->busHashMap.erase(it);
#if DALIA_DEBUG
		m_state->busDebugNames.erase(BusID(identifier));
#endif

		m_state->freeBuses->Push(bIndex);

		DALIA_LOG_DEBUG(LOG_CTX_API, "Destroyed bus %s (index: %d).", identifier, bIndex);
		m_state->rtCommands->Enqueue(RtCommand::DeallocateBus(bIndex));

		return Result::Ok;
	}

	Result Engine::RouteBus(const char* identifier, const char* parentIdentifier) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		// Fetch bus
		uint32_t bIndex;
		Result res = ResolveBusIndex(m_state, identifier, bIndex);
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
		Result resParent = ResolveBusIndex(m_state, parentIdentifier, bIndexParent);
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
			currentAncestor = m_state->busPoolMirror[currentAncestor].parentBusIndex;
		}

		BusMirror& bMirror = m_state->busPoolMirror[bIndex];
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
		uint32_t bIndex;
		Result res = ResolveBusIndex(m_state, identifier, bIndex);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set bus volume. No bus with identifier (%s) exists", identifier);
			return res;
		}

		float clampedVolumeDb = std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX);
		m_state->busPoolMirror[bIndex].volumeDb = clampedVolumeDb;

		m_state->rtCommands->Enqueue(RtCommand::SetBusGain(bIndex, math::DbToGain(clampedVolumeDb)));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Set bus (%s) volume to %.2f dB.", identifier, clampedVolumeDb);

		return Result::Ok;
	}

	Result Engine::CreateBiquadFilter(EffectHandle& effect, BiquadFilterType type, const BiquadConfig& config) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t index, generation;
		if (!m_state->biquadHM->Allocate(index, generation)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create biquad filter. Biquad pool exhausted.");
			return Result::BiquadFilterPoolExhausted;
		}

		effect = EffectHandle::Create(index, generation, EffectType::Biquad);

		// Sanitize config
		BiquadConfig sanitizedConfig;
		sanitizedConfig.frequency = std::clamp(config.frequency, FILTER_FREQUENCY_MIN, FILTER_FREQUENCY_MAX);
		sanitizedConfig.resonance = std::clamp(config.resonance, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX);

		RtCommand cmd = RtCommand::AllocateBiquad(
			index,
			generation,
			type,
			sanitizedConfig
		);
		m_state->rtCommands->Enqueue(cmd);
		DALIA_LOG_DEBUG(LOG_CTX_API, "Created biquad filter with handle uuid: 0x%016llx.", effect.GetUUID());

		return Result::Ok;
	}

	Result Engine::SetBiquadParams(EffectHandle effect, const BiquadConfig& config) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!effect.IsValid()) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set biquad filter parameters. Invalid effect handle.");
			return Result::InvalidHandle;
		}

		if (effect.GetType() != EffectType::Biquad) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set biquad filter parameters. Handle is not of type biquad filter.");
			return Result::InvalidHandle;
		}

		uint32_t index = effect.GetIndex();
		uint32_t gen = effect.GetGeneration();
		if (!m_state->biquadHM->IsValid(index, gen)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to set biquad filter parameters. Expired handle.");
			return Result::ExpiredHandle;
		}

		// Sanitize config
		BiquadConfig sanitizedConfig;
		sanitizedConfig.frequency = std::clamp(config.frequency, FILTER_FREQUENCY_MIN, FILTER_FREQUENCY_MAX);
		sanitizedConfig.resonance = std::clamp(config.resonance, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX);

		m_state->rtCommands->Enqueue(RtCommand::SetBiquadParams(index, gen, sanitizedConfig));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Updated biquad parameters for handle uuid: 0x%016llx.", effect.GetUUID());

		return Result::Ok;
	}

	Result Engine::AttachEffect(EffectHandle effect, const char* busIdentifier, uint32_t effectSlot) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		Result res = ValidateEffectHandle(m_state, effect);
		if (res != Result::Ok) {
			if (res == Result::InvalidHandle) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %d). Invalid handle.",
					busIdentifier, effectSlot);
			}
			else if (res == Result::ExpiredHandle) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to attach effect to bus %s (slot %d). Expired handle.",
					busIdentifier, effectSlot);
			}

			return res;
		}

		// Fetch bus
		uint32_t bIndex;
		Result resBus = ResolveBusIndex(m_state, busIdentifier, bIndex);
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
		auto it = m_state->effectRoutingTable.find(effect.GetUUID());
		if (it != m_state->effectRoutingTable.end()) {
			EffectRouting routing = it->second;
			BusMirror& bMirror = m_state->busPoolMirror[routing.busIndex];
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
		BusMirror& bMirror = m_state->busPoolMirror[bIndex];
		if (bMirror.effectSlots[effectSlot].IsValid()) {
			DALIA_LOG_WARN(LOG_CTX_API, "Detaching effect from bus %s (slot %d). Attaching new effect to slot.",
				busIdentifier, effectSlot);

			// Detach old effect
			EffectHandle oldEffect = bMirror.effectSlots[effectSlot];
			m_state->effectRoutingTable.erase(oldEffect.GetUUID());

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
		m_state->effectRoutingTable[effect.GetUUID()] = EffectRouting(bIndex, effectSlot);

		RtCommand cmd = RtCommand::AttachEffect(
			effect.GetIndex(),
			effect.GetGeneration(),
			effect.GetType(),
			bIndex,
			effectSlot
		);
		m_state->rtCommands->Enqueue(cmd);

		DALIA_LOG_DEBUG(LOG_CTX_API, "Attached effect (handle uuid: 0x%016llx) to bus %s (slot %d).",
			effect.GetUUID(), busIdentifier, effectSlot);

		return Result::Ok;
	}

	Result Engine::DetachEffect(EffectHandle effect) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		Result res = ValidateEffectHandle(m_state, effect);
		if (res != Result::Ok) {
			if (res == Result::InvalidHandle) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to detach effect. Invalid handle.");
			}
			else if (res == Result::ExpiredHandle) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to detach effect. Expired handle.");
			}

			return res;
		}

		// Detach effect if attached (routed)
		auto it = m_state->effectRoutingTable.find(effect.GetUUID());
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
				"Detached effect (uuid: 0x%016llx) from bus (index: %d) (slot: %d).",
				effect.GetUUID(), routing.busIndex, routing.effectSlot);

			return Result::Ok;
		}

		DALIA_LOG_WARN(LOG_CTX_API, "Attempted to detach effect that is not attached.");
		return Result::Ok;
	}

	Result Engine::DestroyEffect(EffectHandle effect) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		Result res = FreeEffectHandle(m_state, effect);
		if (res != Result::Ok) {
			if (res == Result::InvalidHandle) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy effect. Invalid handle.");
			}
			else if (res == Result::ExpiredHandle) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy effect. Expired handle.");
			}

			return res;
		}

		auto it = m_state->effectRoutingTable.find(effect.GetUUID());
		if (it != m_state->effectRoutingTable.end()) {
			EffectRouting routing = it->second;
			BusMirror& bMirror = m_state->busPoolMirror[routing.busIndex];
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

		RtCommand cmd = RtCommand::DeallocateEffect(effect.GetIndex(), effect.GetGeneration(), effect.GetType());
		m_state->rtCommands->Enqueue(cmd);
		DALIA_LOG_DEBUG(LOG_CTX_API, "Destroyed effect (handle uuid: 0x%016llx).", effect.GetUUID());

		return Result::Ok;
	}

	Result Engine::CreatePlayback(PlaybackHandle& pbkHandle, SoundHandle soundHandle, AudioEventCallback callback) {
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
		if (!m_state->freeVoices->Pop(vIndex)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create playback instance. Voice pool exhausted.");
			return Result::VoicePoolExhausted;
		}

		// Prime voice mirror
		VoiceMirror& vMirror = m_state->voicePoolMirror[vIndex];
		vMirror.state = VoiceState::Inactive;
		vMirror.assetUuid = soundHandle.GetUUID();
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
				soundHandle.GetUUID()
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
			uint32_t streamIndex;
			Result streamRes = DispatchStreamPrepare(m_state, streamSound->filepath, streamIndex);
			if (streamRes != Result::Ok) {
				vMirror.Reset();
				m_state->freeVoices->Push(vIndex);

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
				streamIndex,
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
		uint32_t vGeneration = playback.GetGeneration();

		// Fetch voice mirror
		VoiceMirror* vMirror = nullptr;
		Result resVoice = ResolveVoiceMirror(m_state, vIndex, vGeneration, vMirror);
		if (resVoice != Result::Ok) return resVoice;

		// Fetch bus
		uint32_t bIndex;
		const Result resBus = ResolveBusIndex(m_state, busIdentifier, bIndex);
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
		m_state->rtCommands->Enqueue(RtCommand::SetVoiceParent(vIndex, vGeneration, bIndex));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Routed voice %d to bus %s (index: %d).", vIndex, busIdentifier, bIndex);

		return Result::Ok;
	}

	Result Engine::Play(PlaybackHandle handle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = handle.GetIndex();
		uint32_t vGeneration = handle.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, vGeneration, vMirror);
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

		m_state->rtCommands->Enqueue(RtCommand::PlayVoice(vIndex, vGeneration));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to play.", vIndex);

		return Result::Ok;
	}

	Result Engine::Pause(PlaybackHandle playback) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGeneration = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, vGeneration, vMirror);
		if (res != Result::Ok) return res;

		if (vMirror->state != VoiceState::Playing) {
			DALIA_LOG_WARN(LOG_CTX_API, "Calling pause on playback that is not currently playing.");
			return Result::Ok;
		}
		vMirror->state = VoiceState::Paused;
		if (vMirror->pendingLoad) return Result::Ok;

		m_state->rtCommands->Enqueue(RtCommand::PauseVoice(vIndex, vGeneration));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to pause.", vIndex);

		return Result::Ok;
	}

	Result Engine::Stop(PlaybackHandle playback) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t voiceGeneration = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, voiceGeneration, vMirror);
		if (res != Result::Ok) return res;

		// Handle pending playbacks
		if (vMirror->pendingLoad) {
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->voiceIndex == vIndex) {
					if (vMirror->onStopCallback) {
						vMirror->onStopCallback(playback, PlaybackExitCondition::ExplicitStop);
					}

					vMirror->Reset();
					m_state->freeVoices->Push(vIndex);
					DALIA_LOG_DEBUG(LOG_CTX_API, "Stopped voice %d before it started playing.", vIndex);
					m_state->pendingPlaybacks.erase(it);
					return Result::Ok;
				}
				else it++;
			}
		}

		vMirror->state = VoiceState::Stopped;

		m_state->rtCommands->Enqueue(RtCommand::StopVoice(vIndex, voiceGeneration));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to stop.", vIndex);

		return Result::Ok;
	}

	Result Engine::SeekPlayback(PlaybackHandle playback, double timeInSeconds) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGen = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, vGen, vMirror);
		if (res != Result::Ok) return res;

		if (timeInSeconds < 0.0) timeInSeconds = 0.0;

		uint64_t seekFrame64 = static_cast<uint64_t>(timeInSeconds * vMirror->sampleRate);

		// Safety clamps TODO: Log warnings for these
		if (seekFrame64 >= vMirror->frameCount) {
			if (vMirror->frameCount > 0) seekFrame64 = vMirror->frameCount - 1;
			else seekFrame64 = 0;
		}

		uint32_t seekFrame = static_cast<uint32_t>(seekFrame64);
		m_state->rtCommands->Enqueue(RtCommand::SeekVoice(vIndex, vGen, seekFrame));
		DALIA_LOG_DEBUG(LOG_CTX_API, "Seeking voice %d to frame %d.", vIndex, seekFrame);

		return Result::Ok;
	}

	Result Engine::SetPlaybackLooping(PlaybackHandle playback, bool looping) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGeneration = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, vGeneration, vMirror);
		if (res != Result::Ok) return res;

		if (vMirror->isLooping == looping) {
			if (looping) DALIA_LOG_WARN(LOG_CTX_API, "Setting playback to loop. Playback is already set to loop");
			else DALIA_LOG_WARN(LOG_CTX_API, "Setting playback to not loop. Playback is already set to not loop");
			return Result::Ok;
		}

		vMirror->isLooping = looping;
		m_state->rtCommands->Enqueue(RtCommand::SetVoiceLooping(vIndex, vGeneration, looping));
		if (looping) DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to loop.", vIndex);
		else DALIA_LOG_DEBUG(LOG_CTX_API, "Setting voice %d to not loop.", vIndex);

		return Result::Ok;
	}

	Result Engine::SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGeneration = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, vGeneration, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.volumeDb = std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX);
		vMirror->isParamsDirty = true;

		return Result::Ok;
	}

	Result Engine::SetPlaybackStereoPan(PlaybackHandle playback, float pan) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!playback.IsValid()) return Result::InvalidHandle;
		uint32_t vIndex = playback.GetIndex();
		uint32_t vGeneration = playback.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, vIndex, vGeneration, vMirror);
		if (res != Result::Ok) return res;

		vMirror->params.stereoPan = std::clamp(pan, PAN_MIN, PAN_MAX);
		vMirror->isParamsDirty = true;

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
}
