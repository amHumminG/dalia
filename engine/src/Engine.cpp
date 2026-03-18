#include "dalia/audio/Engine.h"
#include "dalia/audio/PlaybackControl.h"

#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"
#include "core/Constants.h"

#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoStreamRequestQueue.h"
#include "messaging/IoLoadRequestQueue.h"
#include "messaging/IoLoadEventQueue.h"

#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"
#include "mixer/BusGraphCompiler.h"

#include "systems/RtSystem.h"
#include "systems/IoStreamSystem.h"
#include "systems/IoLoadSystem.h"

#include "resources/AssetRegistry.h"
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"

#include "StringID.h"

#include <span>

#include "miniaudio.h"

namespace dalia {

	struct MixOrderBuffer {
		std::unique_ptr<uint32_t[]> listA;
		std::unique_ptr<uint32_t[]> listB;

		explicit MixOrderBuffer(uint32_t capacity) {
			listA = std::make_unique<uint32_t[]>(capacity);
			listB = std::make_unique<uint32_t[]>(capacity);
		}
	};

	struct PendingSoundUnload {
		SoundHandle handle;
		std::vector<PlaybackHandle> voicesToStop;
	};

	struct PendingPlayback {
		uint32_t voiceIndex;
		uint32_t voiceGeneration;
		uint64_t assetUuid;
	};

	struct EngineInternalState {
		bool initialized = false;

		// Miniaudio
		std::unique_ptr<ma_device> device;

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

		// --- Pools ---
		std::unique_ptr<Voice[]>				voicePool;
		std::unique_ptr<StreamContext[]>		streamPool;
		std::unique_ptr<Bus[]>					busPool;
		std::unique_ptr<float[]>				busMemoryPool;
		Bus* masterBus = nullptr;

		// --- Mirrors ---
		std::unique_ptr<VoiceMirror[]>		voicePoolMirror;
		std::unique_ptr<BusMirror[]>		busPoolMirror;

		// --- Availability Containers ---
		std::unique_ptr<FixedStack<uint32_t>>		freeVoices;
		std::unique_ptr<SPSCRingBuffer<uint32_t>>	freeStreams;
		std::unique_ptr<FixedStack<uint32_t>>		freeBuses;

		// -- Bus Execution Graph ---
		std::unique_ptr<MixOrderBuffer>		mixOrderBuffer;
		std::unique_ptr<BusGraphCompiler>	busGraphCompiler;

		bool isUsingMixOrderA		= true;
		bool isMixOrderDirty		= false;
		bool isMixOrderSwapPending	= false;

		std::unique_ptr<RtSystem> rtSystem;
		std::unique_ptr<IoStreamSystem> ioStreamSystem;
		std::unique_ptr<IoLoadSystem> ioLoadSystem;

		// --- Resources ---
		std::unique_ptr<AssetRegistry> assetRegistry;
		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> loadCallbacks;

		// Deferred Unloads
		std::vector<PendingSoundUnload> pendingSoundUnloads;

		// Deferred playback
		std::vector<PendingPlayback> pendingPlaybacks;

		uint32_t GenerateIoLoadRequestId() {return nextIoLoadRequestId++; }

		EngineInternalState(const EngineConfig& config)
			: voiceCapacity(config.voiceCapacity), streamCapacity(config.streamCapacity), busCapacity(config.busCapacity) {
			// Message Queues
			rtCommands			= std::make_unique<RtCommandQueue>(config.rtCommandQueueCapacity);
			rtEvents			= std::make_unique<RtEventQueue>(config.rtEventQueueCapacity);
			ioStreamRequests	= std::make_unique<IoStreamRequestQueue>(config.ioStreamRequestQueueCapacity);
			ioLoadRequests		= std::make_unique<IoLoadRequestQueue>(config.ioLoadRequestQueueCapacity);
			ioLoadEvents		= std::make_unique<IoLoadEventQueue>(config.ioLoadEventQueueCapacity);

			// Pools
			voicePool		= std::make_unique<Voice[]>(voiceCapacity);
			voicePoolMirror	= std::make_unique<VoiceMirror[]>(voiceCapacity);
			streamPool		= std::make_unique<StreamContext[]>(streamCapacity);
			busPool			= std::make_unique<Bus[]>(busCapacity);
			busPoolMirror	= std::make_unique<BusMirror[]>(busCapacity);

			// Bus graph
			mixOrderBuffer		= std::make_unique<MixOrderBuffer>(busCapacity);
			busGraphCompiler	= std::make_unique<BusGraphCompiler>(busCapacity);

			// Availability Containers
			freeVoices	= std::make_unique<FixedStack<uint32_t>>(voiceCapacity);
			freeStreams = std::make_unique<SPSCRingBuffer<uint32_t>>(streamCapacity);
			freeBuses	= std::make_unique<FixedStack<uint32_t>>(busCapacity);
			for (int i = voiceCapacity - 1; i >= 0; i--)	freeVoices->Push(i);
			for (int i = streamCapacity - 1; i >= 0; i--)	freeStreams->Push(i);
			for (int i = busCapacity - 1; i >= 0; i--)		freeBuses->Push(i); // Skip Master (index 0)

			// Resources
			assetRegistry = std::make_unique<AssetRegistry>(config.residentSoundCapacity, config.streamSoundCapacity);
		}
	};

	// --- INTERNAL HELPERS ---
	static inline bool IsInitialized(const EngineInternalState* state) {
		return state != nullptr && state->initialized;
	}

	static inline Result ResolveVoiceMirror(EngineInternalState* state, uint32_t index, uint32_t generation, VoiceMirror*& outMirror) {
		if (index >= state->voiceCapacity) return Result::InvalidHandle;

		VoiceMirror* mirror = &state->voicePoolMirror[index];
		if (mirror->generation != generation) return Result::ExpiredHandle;

		outMirror = mirror;
		return Result::Ok;
	}

	static inline Result DispatchStreamPrepare(EngineInternalState* state, const char* filepath, uint32_t& streamIndex) {
		if (!state->freeStreams->Pop(streamIndex)) {
			Logger::Log(LogLevel::Error, "Engine", "Failed to prepare stream instance. Stream pool exhausted.");
			return Result::StreamPoolExhausted;
		}

		// Send I/O request to prepare stream
		state->streamPool[streamIndex].state.store(StreamState::Preparing, std::memory_order_release);
		IoStreamRequest req = IoStreamRequest::PrepareStream(streamIndex, filepath);
		if (!state->ioStreamRequests->Push(req)) {
			// Rollback
			state->streamPool[streamIndex].state.store(StreamState::Free, std::memory_order_release);
			state->freeStreams->Push(streamIndex);

			Logger::Log(LogLevel::Error, "Engine", "Failed to prepare stream instance. IO Request queue full.");
			return Result::IoRequestQueueFull;
		}

		return Result::Ok;
	}

	// ------------------------

	Engine::Engine() = default;
	Engine::~Engine() { if (m_state) delete m_state; };

	Result Engine::Init(const EngineConfig& config) {
		if (m_state != nullptr) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to initialize engine that is already initialized");
			return Result::AlreadyInitialized;
		}

		Logger::Init(config.logLevel, 256);

		// --- INTERNAL STATE SETUP ---
		m_state = new EngineInternalState(config);

		// Bus buffer allocation and assignment
		// Setup bus memory pool (Room for 1024 frames) maybe we should check this against m_periodSize?
		constexpr uint32_t samplesPerBus = 1024 * CHANNELS_STEREO;
		m_state->busMemoryPool = std::make_unique<float[]>(m_state->busCapacity * samplesPerBus);

		// Bus buffer assigment
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			float* busStart = &m_state->busMemoryPool[i * samplesPerBus];
			m_state->busPool[i].SetBuffer(std::span<float>(busStart, samplesPerBus));
		}

		m_state->masterBus = &m_state->busPool[MASTER_BUS_INDEX];
		m_state->masterBus->SetName("Master");
		m_state->busPoolMirror[MASTER_BUS_INDEX].isBusy = true;

		m_state->isMixOrderDirty = true;
		m_state->isMixOrderSwapPending = false;
		TryUpdateMixOrder();

		// --- SYSTEMS SETUP ---
		RtSystemConfig rtConfig;
		rtConfig.rtCommands		= m_state->rtCommands.get();
		rtConfig.rtEvents		= m_state->rtEvents.get();
		rtConfig.ioStreamRequests = m_state->ioStreamRequests.get();
		rtConfig.voicePool		= std::span<Voice>(m_state->voicePool.get(), m_state->voiceCapacity);
		rtConfig.streamPool		= std::span<StreamContext>(m_state->streamPool.get(), m_state->streamCapacity);
		rtConfig.busPool		= std::span<Bus>(m_state->busPool.get(), m_state->busCapacity);
		rtConfig.masterBus		= m_state->masterBus;
		m_state->rtSystem = std::make_unique<RtSystem>(rtConfig);

		IoStreamSystemConfig ioStreamingConfig;
		ioStreamingConfig.ioStreamRequests	= m_state->ioStreamRequests.get();
		ioStreamingConfig.streamPool		= std::span<StreamContext>(m_state->streamPool.get(), m_state->streamCapacity);
		ioStreamingConfig.freeStreams		= m_state->freeStreams.get();
		m_state->ioStreamSystem				= std::make_unique<IoStreamSystem>(ioStreamingConfig);

		IoLoadSystemConfig ioLoadSystemConfig;
		ioLoadSystemConfig.ioLoadRequests = m_state->ioLoadRequests.get();
		ioLoadSystemConfig.ioLoadEvents = m_state->ioLoadEvents.get();
		ioLoadSystemConfig.assetRegistry = m_state->assetRegistry.get();
		m_state->ioLoadSystem = std::make_unique<IoLoadSystem>(ioLoadSystemConfig);

		// --- BACKEND SETUP ---
		m_state->device					= std::make_unique<ma_device>();
		ma_device_config deviceConfig	= ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format	= ma_format_f32;
		deviceConfig.playback.channels	= CHANNELS_STEREO; // Stereo playback only
		deviceConfig.sampleRate			= 48000; // Hz
		deviceConfig.dataCallback		= data_callback;
		deviceConfig.periodSizeInFrames = 480;
		deviceConfig.pUserData			= m_state->rtSystem.get();

		if (ma_device_init(NULL, &deviceConfig, m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to initialize playback device");
			delete m_state;
			m_state = nullptr;
			return Result::DeviceFailed;
		}

		// --- SYSTEMS START ---
		// I/O Thread Start
		m_state->ioStreamSystem->Start();
		m_state->ioLoadSystem->Start();

		// Audio Thread Start
		if (ma_device_start(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to start playback device");
			delete m_state;
			m_state = nullptr;
			return Result::DeviceFailed;
		}

		m_state->initialized = true;
		Logger::Log(LogLevel::Info, "Engine", "Initialized engine");
		return Result::Ok;
	}

	Result Engine::Deinit() {
		if (!IsInitialized(m_state)) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to deinitialize engine that is not initialized");
			return Result::NotInitialized;
		}

		// --- Audio Thread Stop ---
		if (ma_device_stop(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to stop playback device");
			return Result::DeviceFailed;
		}
		ma_device_uninit(m_state->device.get());

		// --- I/O Thread Stop ---
		m_state->ioLoadSystem->Stop();
		m_state->ioStreamSystem->Stop();

		delete m_state;
		m_state = nullptr;

		Logger::Log(LogLevel::Info, "Engine", "Deinitialized engine");
		Logger::Deinit();

		return Result::Ok;
	}

	void Engine::Update() {
		if (!IsInitialized(m_state)) return;

		// --- Process Event Inbox ---
		RtEvent RtEv;
		while (m_state->rtEvents->Pop(RtEv)) {
			ProcessRtEvent(RtEv);
		}

		IoLoadEvent loadEv;
		while (m_state->ioLoadEvents->Pop(loadEv)) {
			ProcessIoLoadEvent(loadEv);
		}

		TryUpdateMixOrder();

		m_state->rtCommands->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}

	Result Engine::LoadSound(SoundHandle& soundHandle, SoundType soundType, const char* filepath, AssetLoadCallback callback,
		uint32_t* outRequestId) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		StringID pathId(filepath);

		// Duplicate check
		if (m_state->assetRegistry->GetLoadedSoundHandle(pathId, soundHandle)) {
			if (soundHandle.GetType() == soundType) {
				if (soundType == SoundType::Resident) {
					ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
					if (sound) sound->refCount++;
				}
				else if (soundType == SoundType::Stream) {
					StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
					if (sound) sound->refCount++;
				}

				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (outRequestId) *outRequestId = INVALID_REQUEST_ID;

				return Result::Ok;
			}
		}

		soundHandle = m_state->assetRegistry->AllocateSound(soundType);
		if (!soundHandle.IsValid()) {
			if (soundType == SoundType::Resident) {
				Logger::Log(LogLevel::Error, "Engine", "Failed to load sound. Resident sound pool exhausted.");
				return Result::ResidentSoundPoolExhausted;
			}
			else if (soundType == SoundType::Stream) {
				Logger::Log(LogLevel::Error, "Engine", "Failed to load sound. Stream sound pool exhausted.");
				return Result::StreamSoundPoolExhausted;
			}
		}

		if (soundType == SoundType::Resident) {
			ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
			sound->refCount = 1;
			sound->pathHash = pathId.GetHash();
			sound->state.store(LoadState::Loading, std::memory_order_relaxed);
		}
		else if (soundType == SoundType::Stream) {
			StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
			sound->refCount = 1;
			sound->pathHash = pathId.GetHash();
			sound->state.store(LoadState::Loading, std::memory_order_relaxed);
		}
		m_state->assetRegistry->RegisterLoadedSound(pathId, soundHandle);

		uint32_t requestId = m_state->GenerateIoLoadRequestId();
		if (outRequestId) *outRequestId = requestId;
		if (callback) m_state->loadCallbacks[requestId] = std::move(callback);

		m_state->ioLoadRequests->Push(IoLoadRequest::LoadSound(requestId, soundHandle, filepath));

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
			m_state->assetRegistry->UnregisterLoadedSound(StringID::FromHash(soundPathHash));

			PendingSoundUnload pendingUnload;
			pendingUnload.handle = soundHandle;

			// Remove pending playbacks for the sound
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->assetUuid == soundHandle.GetUUID()) {
					VoiceMirror& vMirror = m_state->voicePoolMirror[it->voiceIndex];

					vMirror.Reset();
					m_state->freeVoices->Push(it->voiceIndex);

					Logger::Log(LogLevel::Debug, "Engine", "Aborted deferred playback for voice %d due to unload.",
						it->voiceIndex);
					it = m_state->pendingPlaybacks.erase(it);
				}
				else it++;
			}

			// Command active voices using the asset to stop
			for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
				VoiceMirror& vMirror = m_state->voicePoolMirror[i];

				if (vMirror.state != VoiceState::Free && vMirror.assetUuid == soundHandle.GetUUID()) {
					pendingUnload.voicesToStop.push_back(PlaybackHandle::Create(i, vMirror.generation));

					m_state->rtCommands->Enqueue(RtCommand::StopVoice(i, vMirror.generation));
					Logger::Log(LogLevel::Debug, "Engine", "Commanded to stop voice: %d", i);
					}
			}

			if (!pendingUnload.voicesToStop.empty()) {
				m_state->pendingSoundUnloads.push_back(std::move(pendingUnload));
			}
			else {
				m_state->assetRegistry->FreeSound(soundHandle);
				Logger::Log(LogLevel::Debug, "Engine", "Unloaded sound with handle %d (instant)",
					soundHandle.GetUUID());
			}
		}

		return Result::Ok;
	}

	Result Engine::CreatePlayback(PlaybackHandle& pbkHandle, SoundHandle soundHandle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = soundHandle.GetType();
		ResidentSound* residentSound = nullptr;
		StreamSound* streamSound = nullptr;
		LoadState soundLoadState;

		if (soundType == SoundType::Resident) {
			residentSound = m_state->assetRegistry->GetResidentSound(soundHandle);
			if (!residentSound) return Result::InvalidHandle;
			soundLoadState = residentSound->state.load(std::memory_order_acquire);
		}
		else {
			streamSound = m_state->assetRegistry->GetStreamSound(soundHandle);
			if (!streamSound) return Result::InvalidHandle;
			soundLoadState = streamSound->state.load(std::memory_order_acquire);
		}

		if (soundLoadState == LoadState::Error) {
			Logger::Log(LogLevel::Error, "Engine", "Failed to play sound. Sound loading failed.");
			return Result::SoundLoadError;
		}

		uint32_t voiceIndex;
		if (!m_state->freeVoices->Pop(voiceIndex)) {
			Logger::Log(LogLevel::Error, "Engine", "Failed to create playback instance. Voice pool exhausted.");
			return Result::VoicePoolExhausted;
		}

		// Prime voice mirror
		VoiceMirror& vMirror = m_state->voicePoolMirror[voiceIndex];
		vMirror.generation++;
		vMirror.state = VoiceState::Inactive;
		vMirror.soundType = soundType;
		vMirror.assetUuid = soundHandle.GetUUID();

		// Deferred playback
		if (soundLoadState == LoadState::Loading) {
			m_state->pendingPlaybacks.push_back({
				voiceIndex,
				vMirror.generation,
				soundHandle.GetUUID()
			});
			vMirror.pendingLoad = true;
			Logger::Log(LogLevel::Debug, "Engine", "Deferring playback for voice %d. Sound not yet loaded.",
				voiceIndex);

			pbkHandle = PlaybackHandle::Create(voiceIndex, vMirror.generation);
			return Result::Ok;
		}

		if (soundType == SoundType::Resident) {
			RtCommand cmd = RtCommand::PrepareVoiceResident(
				voiceIndex,
				vMirror.generation,
				residentSound->pcmData.data(),
				residentSound->totalFrames,
				residentSound->channels,
				residentSound->sampleRate
			);
			m_state->rtCommands->Enqueue(cmd);
		}
		else if (soundType == SoundType::Stream) {

			uint32_t streamIndex;
			Result streamResult = DispatchStreamPrepare(m_state, streamSound->filepath, streamIndex);
			if (streamResult != Result::Ok) {
				vMirror.Reset();
				m_state->freeVoices->Push(voiceIndex);
				return streamResult;
			}

			RtCommand cmd = RtCommand::PrepareVoiceStreaming(
				voiceIndex,
				vMirror.generation,
				streamIndex,
				streamSound->channels,
				streamSound->sampleRate
			);
			m_state->rtCommands->Enqueue(cmd);
		}

		pbkHandle = PlaybackHandle::Create(voiceIndex, vMirror.generation);
		return Result::Ok;
	}

	Result Engine::Play(PlaybackHandle handle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;
		uint32_t voiceIndex = handle.GetIndex();
		uint32_t voiceGeneration = handle.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, voiceIndex, voiceGeneration, vMirror);
		if (res != Result::Ok) return res;

		// Deffered playback
		if (vMirror->pendingLoad) {
			vMirror->state = VoiceState::Playing;
			Logger::Log(LogLevel::Warning, "Engine",
				"Calling play on asset that is still loading. Will play when finished loading.");
			return Result::Ok;
		}

		// Check for double calls
		if (vMirror->state == VoiceState::Playing) {
			Logger::Log(LogLevel::Warning, "Engine", "Calling play on handle already set to play.");
			return Result::Ok;
		}

		if (vMirror->state != VoiceState::Inactive && vMirror->state != VoiceState::Paused) {
			return Result::PlaybackCorrupted;
		}

		vMirror->state = VoiceState::Playing;
		RtCommand cmd = RtCommand::PlayVoice(voiceIndex, voiceGeneration);
		m_state->rtCommands->Enqueue(cmd);

		return Result::Ok;
	}

	Result Engine::Pause(PlaybackHandle handle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;
		uint32_t voiceIndex = handle.GetIndex();
		uint32_t voiceGeneration = handle.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, voiceIndex, voiceGeneration, vMirror);
		if (res != Result::Ok) return res;

		if (vMirror->state != VoiceState::Playing) {
			Logger::Log(LogLevel::Warning, "Engine", "Failed to pause voice %d. Not currently playing.", voiceIndex);
			return Result::Ok;
		}
		vMirror->state = VoiceState::Paused;
		if (vMirror->pendingLoad) return Result::Ok;

		m_state->rtCommands->Enqueue(RtCommand::PauseVoice(voiceIndex, voiceGeneration));
		return Result::Ok;
	}

	Result Engine::Stop(PlaybackHandle handle) {
		if (!IsInitialized(m_state)) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;
		uint32_t voiceIndex = handle.GetIndex();
		uint32_t voiceGeneration = handle.GetGeneration();

		VoiceMirror* vMirror = nullptr;
		Result res = ResolveVoiceMirror(m_state, voiceIndex, voiceGeneration, vMirror);
		if (res != Result::Ok) return res;

		// Handle pending playbacks
		if (vMirror->pendingLoad) {
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->voiceIndex == voiceIndex) {
					vMirror->Reset();
					m_state->freeVoices->Push(voiceIndex);
					Logger::Log(LogLevel::Debug, "Engine",
					"Stopped voice %d before it started playing.", voiceIndex);
					it = m_state->pendingPlaybacks.erase(it);
					return Result::Ok;
				}
				else it++;
			}
		}

		vMirror->state = VoiceState::Stopped;
		m_state->rtCommands->Enqueue(RtCommand::StopVoice(voiceIndex, voiceGeneration));

		return Result::Ok;
	}

	void Engine::ProcessRtEvent(const RtEvent& ev) {
		switch (ev.type) {
			case RtEvent::Type::MixOrderSwapped: {
				m_state->isUsingMixOrderA = !m_state->isUsingMixOrderA;
				m_state->isMixOrderSwapPending = false;
				break;
			}
			case RtEvent::Type::VoiceStopped: {
				uint32_t index = ev.data.voiceState.index;
				uint32_t generation = ev.data.voiceState.generation;
				if (m_state->voicePoolMirror[index].generation == generation) {
					// Voice is still valid -> Return it to the pool
					m_state->voicePoolMirror[index].Reset();
					m_state->freeVoices->Push(index);
					Logger::Log(LogLevel::Debug, "Engine", "Freed voice %d.", index);

					// --- Check garbage collection ---
					PlaybackHandle stoppedVoice = PlaybackHandle::Create(index, generation);
					for (auto it = m_state->pendingSoundUnloads.begin(); it != m_state->pendingSoundUnloads.end(); ) {
						auto& waitingList = it->voicesToStop;

						for (size_t i = 0; i < waitingList.size(); i++) {
							if (waitingList[i] == stoppedVoice) {
								waitingList[i] = waitingList.back();
								waitingList.pop_back();
								break;
							}
						}

						if (waitingList.empty()) {
							m_state->assetRegistry->FreeSound(it->handle);

							const char* typeStr = (it->handle.GetType() == SoundType::Resident) ? "resident" : "stream";
							Logger::Log(LogLevel::Debug, "Engine", "Unloaded %s sound with handle %d (deferred)",
								typeStr, it->handle.GetUUID());

							it = m_state->pendingSoundUnloads.erase(it);
						}
						else it++;
					}
				}
				break;
			}
		}
	}

	void Engine::ProcessIoLoadEvent(const IoLoadEvent& ev) {
		// Execute user-registered callback
		auto it = m_state->loadCallbacks.find(ev.requestId);
		if (it != m_state->loadCallbacks.end()) {
			if (it->second) {
				it->second(ev.requestId, ev.result); // Call it
			}
			m_state->loadCallbacks.erase(it);
		}

		switch (ev.type) {
			case IoLoadEvent::Type::SoundLoaded: {
				// Process deferred playbacks
				for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
					if (it->assetUuid == ev.assetUuid) {
						VoiceMirror* vMirror = nullptr;
						Result res = ResolveVoiceMirror(m_state, it->voiceIndex, it->voiceGeneration, vMirror);

						if (res == Result::Ok && vMirror->pendingLoad) {
							SoundHandle handle = SoundHandle::FromUUID(ev.assetUuid);
							SoundType soundType = handle.GetType();

							if (soundType == SoundType::Resident) {
								ResidentSound* sound = m_state->assetRegistry->GetResidentSound(handle);

								RtCommand cmd = RtCommand::PrepareVoiceResident(
									it->voiceIndex,
									it->voiceGeneration,
									sound->pcmData.data(),
									sound->totalFrames,
									sound->channels,
									sound->sampleRate
								);
								m_state->rtCommands->Enqueue(cmd);
							}
							else if (soundType == SoundType::Stream) {
								StreamSound* sound = m_state->assetRegistry->GetStreamSound(handle);

								uint32_t streamIndex;
								Result streamResult = DispatchStreamPrepare(m_state, sound->filepath, streamIndex);
								if (streamResult != Result::Ok) {
									vMirror->Reset();
									m_state->freeVoices->Push(it->voiceIndex);

									Logger::Log(LogLevel::Error, "Engine",
										"Aborting deferred playback. Stream preparation failed");
									it = m_state->pendingPlaybacks.erase(it);
									continue;
								}

								RtCommand cmd = RtCommand::PrepareVoiceStreaming(
									it->voiceIndex,
									vMirror->generation,
									streamIndex,
									sound->channels,
									sound->sampleRate
								);
								m_state->rtCommands->Enqueue(cmd);
							}

							vMirror->pendingLoad = false;
							if (vMirror->state == VoiceState::Playing) {
								m_state->rtCommands->Enqueue(RtCommand::PlayVoice(it->voiceIndex, it->voiceGeneration));
							}
							else if (vMirror->state == VoiceState::Paused) {
								m_state->rtCommands->Enqueue(RtCommand::PauseVoice(it->voiceIndex, it->voiceGeneration));
							}
							else if (vMirror->state == VoiceState::Stopped) {
								m_state->rtCommands->Enqueue(RtCommand::StopVoice(it->voiceIndex, it->voiceGeneration));
							}
						}

						it = m_state->pendingPlaybacks.erase(it);
					}
					else {
						it++;
					}
				}
				break;
			}
			case IoLoadEvent::Type::SoundLoadFailed: {
				for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
					if (it->assetUuid == ev.assetUuid) {
						VoiceMirror* vMirror = nullptr;
						Result res = ResolveVoiceMirror(m_state, it->voiceIndex, it->voiceGeneration, vMirror);

						if (res == Result::Ok) {
							Logger::Log(LogLevel::Warning, "Engine",
								"Aborting deferred playback. Asset failed to load.");
							vMirror->Reset();
							m_state->freeVoices->Push(it->voiceIndex);
						}

						it = m_state->pendingPlaybacks.erase(it);
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

	void Engine::TryUpdateMixOrder() {
		if (!m_state->isMixOrderDirty || m_state->isMixOrderSwapPending) return;

		uint32_t* backBufferPtr = m_state->isUsingMixOrderA
		? m_state->mixOrderBuffer->listB.get()
		: m_state->mixOrderBuffer->listA.get();

		std::span<uint32_t> backBufferSpan(backBufferPtr, m_state->busCapacity);
		std::span<const BusMirror> busMirror(m_state->busPoolMirror.get(), m_state->busCapacity);

		uint32_t sortedCount = m_state->busGraphCompiler->Compile(busMirror, backBufferSpan);
		if (sortedCount > 0) {
			m_state->rtCommands->Enqueue(RtCommand::SwapMixOrder(backBufferPtr, sortedCount));

			m_state->isMixOrderSwapPending = true;
			m_state->isMixOrderDirty = false;
		}
		else {
			Logger::Log(LogLevel::Critical, "Bus Routing", "Failed to compile mix graph: Cycle detected.");
			m_state->isMixOrderDirty = false;
		}
	}

	void Engine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
		RtSystem* rtSystem = static_cast<RtSystem*>(pDevice->pUserData);
		rtSystem->OnAudioCallback(static_cast<float*>(pOutput), frameCount, pDevice->playback.channels);
	}
}