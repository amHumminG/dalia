#include "dalia/audio/Engine.h"
#include "dalia/audio/PlaybackControl.h"

#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"

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

	static constexpr uint8_t CHANNELS_STEREO = 2;
	static constexpr uint32_t MASTER_BUS_INDEX = 0;

	struct MixOrderBuffer {
		std::unique_ptr<uint32_t[]> listA;
		std::unique_ptr<uint32_t[]> listB;

		explicit MixOrderBuffer(uint32_t capacity) {
			listA = std::make_unique<uint32_t[]>(capacity);
			listB = std::make_unique<uint32_t[]>(capacity);
		}
	};

	struct VoiceID {
		uint32_t index;
		uint32_t generation;

		bool operator==(const VoiceID& other) const {
			return index == other.index && generation == other.generation;
		}
	};

	struct PendingResidentUnload {
		ResidentSoundHandle handle;
		std::vector<VoiceID> voicesToStop;
	};

	struct PendingStreamUnload {
		StreamSoundHandle handle;
		std::vector<VoiceID> voicesToStop;
	};

	struct PendingPlayback {
		uint32_t voiceIndex;
		uint32_t voiceGeneration;
		uint64_t assetUuid;
		VoiceSourceType sourceType;
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

		std::vector<PendingResidentUnload> pendingResidentUnloads;
		std::vector<PendingStreamUnload> pendingStreamUnloads;

		std::unique_ptr<RtSystem> rtSystem;
		std::unique_ptr<IoStreamSystem> ioStreamSystem;
		std::unique_ptr<IoLoadSystem> ioLoadSystem;

		// --- Resources ---
		std::unique_ptr<AssetRegistry> assetRegistry;
		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> loadCallbacks;

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


	Engine::Engine() = default;
	Engine::~Engine() = default;

	Result Engine::Init(const EngineConfig& config) {
		Logger::Init(config.logLevel, 256);

		if (m_state) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to initialize engine that is already initialized");
			return Result::AlreadyInitialized;
		}

		// --- INTERNAL STATE SETUP ---
		m_state = std::make_unique<EngineInternalState>(config);

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
			return Result::DeviceFailed;
		}

		// --- SYSTEMS START ---
		// I/O Thread Start
		m_state->ioStreamSystem->Start();
		m_state->ioLoadSystem->Start();

		// Audio Thread Start
		if (ma_device_start(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to start playback device");
			return Result::DeviceFailed;
		}

		m_state->initialized = true;
		Logger::Log(LogLevel::Info, "Engine", "Initialized engine");
		return Result::Ok;
	}

	Result Engine::Deinit() {
		if (!m_state) {
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
		m_state->ioStreamSystem->Stop();

		m_state.reset();

		Logger::Log(LogLevel::Info, "Engine", "Deinitialized engine");
		Logger::Deinit();

		return Result::Ok;
	}

	void Engine::Update() {
		if (!m_state->initialized) return;

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

	Result Engine::LoadResidentSound(ResidentSoundHandle& soundHandle, const char* filepath,
		AssetLoadCallback callback, uint32_t* outRequestId) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		StringID pathId(filepath);

		// Duplicate check
		if (m_state->assetRegistry->GetLoadedResidentSoundHandle(pathId, soundHandle)) {
			ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
			if (sound) {
				sound->refCount++;

				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (outRequestId) *outRequestId = INVALID_REQUEST_ID;

				return Result::Ok;
			}
		}

		soundHandle = m_state->assetRegistry->AllocateResident();
		if (!soundHandle.IsValid()) {
			Logger::Log(LogLevel::Warning, "Engine", "Failed to load resident sound. Invalid handle.");
			return Result::Error; // TODO: Return descriptive error
		}

		ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
		sound->refCount = 1;
		sound->pathHash = pathId.GetHash();
		sound->state.store(LoadState::Loading, std::memory_order_relaxed);

		m_state->assetRegistry->RegisterLoadedResidentSound(pathId, soundHandle);

		uint32_t requestId = m_state->GenerateIoLoadRequestId();
		if (outRequestId) *outRequestId = requestId;
		if (callback) m_state->loadCallbacks[requestId] = std::move(callback);

		m_state->ioLoadRequests->Push(IoLoadRequest::LoadResidentSound(requestId, soundHandle, filepath));

		return Result::Ok;
	}

	Result Engine::LoadStreamSound(StreamSoundHandle& soundHandle, const char* filepath,
		AssetLoadCallback callback, uint32_t* outRequestId) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		StringID pathId(filepath);

		// Duplicate check
		if (m_state->assetRegistry->GetLoadedStreamSoundHandle(pathId, soundHandle)) {
			StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
			if (sound) {
				sound->refCount++;

				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (outRequestId) *outRequestId = INVALID_REQUEST_ID;

				return Result::Ok;
			}
		}

		soundHandle = m_state->assetRegistry->AllocateStreamSound();
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		if (!soundHandle.IsValid()) {
			Logger::Log(LogLevel::Warning, "Engine", "Failed to load stream sound. Invalid handle.");
			return Result::Error; // TODO: Return descriptive error
		}


		StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
		sound->refCount = 1;
		sound->pathHash = pathId.GetHash();
		sound->state.store(LoadState::Loading, std::memory_order_relaxed);

		m_state->assetRegistry->RegisterLoadedStreamSound(pathId, soundHandle);

		uint32_t requestId = m_state->GenerateIoLoadRequestId();
		if (outRequestId) *outRequestId = requestId;
		if (callback) m_state->loadCallbacks[requestId] = std::move(callback);

		m_state->ioLoadRequests->Push(IoLoadRequest::LoadStreamSound(requestId, soundHandle, filepath));

		return Result::Ok;
	}

	Result Engine::Unload(ResidentSoundHandle soundHandle) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
		if (!sound) return Result::InvalidHandle;

		if (sound->refCount > 0) sound->refCount--;

		if (sound->refCount == 0) {
			m_state->assetRegistry->UnregisterLoadedResidentSound(StringID::FromHash(sound->pathHash));

			// Collect all voice indices using the handle
			// TODO: Find a more efficient way to do this (time complexity wise)
			PendingResidentUnload pendingUnload;
			pendingUnload.handle = soundHandle;
			for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
				VoiceMirror& vMirror = m_state->voicePoolMirror[i];

				if (vMirror.state != VoiceState::Free &&
					vMirror.assetUuid == soundHandle.GetUUID() &&
					vMirror.sourceType == VoiceSourceType::Resident) {
					pendingUnload.voicesToStop.push_back({i, vMirror.generation});

					m_state->rtCommands->Enqueue(RtCommand::StopVoice(i, vMirror.generation));
					Logger::Log(LogLevel::Debug, "Engine", "Commanded to stop voice: %d", i);
				}
			}

			if (!pendingUnload.voicesToStop.empty()) {
				m_state->pendingResidentUnloads.push_back(std::move(pendingUnload));
			}
			else {
				m_state->assetRegistry->FreeResidentSound(soundHandle);
				Logger::Log(LogLevel::Debug, "Engine", "Unloaded resident sound with handle %d (instant)",
					soundHandle.GetUUID());
			}
		}

		return Result::Ok;
	}

	Result Engine::Unload(StreamSoundHandle soundHandle) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
		if (!sound) return Result::InvalidHandle;

		if (sound->refCount > 0) sound->refCount--;

		if (sound->refCount == 0) {
			m_state->assetRegistry->UnregisterLoadedStreamSound(StringID::FromHash(sound->pathHash));


			// Collect all voice indices using the handle
			// TODO: Find a more efficient way to do this (time complexity wise)
			PendingStreamUnload pendingUnload;
			pendingUnload.handle = soundHandle;
			for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
				VoiceMirror& vMirror = m_state->voicePoolMirror[i];

				if (vMirror.state != VoiceState::Free &&
					vMirror.assetUuid == soundHandle.GetUUID() &&
					vMirror.sourceType == VoiceSourceType::Stream) {
					pendingUnload.voicesToStop.push_back({i, vMirror.generation});

					m_state->rtCommands->Enqueue(RtCommand::StopVoice(i, vMirror.generation));
				}
			}

			if (!pendingUnload.voicesToStop.empty()) {
				m_state->pendingStreamUnloads.push_back(std::move(pendingUnload));
			}
			else {
				m_state->assetRegistry->FreeStreamSound(soundHandle);
				Logger::Log(LogLevel::Debug, "Engine", "Unloaded stream sound with handle %d (instant)",
					soundHandle.GetUUID());
			}
		}

		return Result::Ok;
	}

	Result Engine::CreatePlayback(PlaybackHandle& pbkHandle, ResidentSoundHandle soundHandle) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		ResidentSound* sound = m_state->assetRegistry->GetResidentSound(soundHandle);
		if (!sound) return Result::InvalidHandle;

		LoadState currentLoadState = sound->state.load(std::memory_order_acquire);
		if (currentLoadState == LoadState::Error) {
			Logger::Log(LogLevel::Warning, "Engine", "Failed to play resident sound. Not finished loading.");
			return Result::AssetLoadError;
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
		vMirror.sourceType = VoiceSourceType::Resident;
		vMirror.assetUuid = soundHandle.GetUUID();

		// Deferred playback
		if (currentLoadState == LoadState::Loading) {
			m_state->pendingPlaybacks.push_back({
				voiceIndex,
				vMirror.generation,
				soundHandle.GetUUID(),
				VoiceSourceType::Resident
			});
			vMirror.state = VoiceState::PendingLoadInactive;
			Logger::Log(LogLevel::Debug, "Engine", "Deferring resident playback for voice %d. Asset not yet loaded.",
				voiceIndex);

			pbkHandle = PlaybackHandle::Create(voiceIndex, vMirror.generation);
			return Result::Ok;
		}

		RtCommand cmd = RtCommand::PrepareVoiceResident(
			voiceIndex,
			vMirror.generation,
			sound->pcmData.data(),
			sound->totalFrames,
			sound->channels,
			sound->sampleRate
		);
		m_state->rtCommands->Enqueue(cmd);

		pbkHandle = PlaybackHandle::Create(voiceIndex, vMirror.generation);
		return Result::Ok;
	}

	Result Engine::CreatePlayback(PlaybackHandle& pbkHandle, StreamSoundHandle soundHandle) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		StreamSound* sound = m_state->assetRegistry->GetStreamSound(soundHandle);
		if (!sound) return Result::InvalidHandle;

		LoadState currentLoadState = sound->state.load(std::memory_order_acquire);
		if (currentLoadState == LoadState::Error) {
			Logger::Log(LogLevel::Error, "Engine", "Failed to play resident sound. Asset loading failed.");
			return Result::AssetLoadError;
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
		vMirror.sourceType = VoiceSourceType::Stream;
		vMirror.assetUuid = soundHandle.GetUUID();

		// Deferred playback
		if (currentLoadState == LoadState::Loading) {
			m_state->pendingPlaybacks.push_back({
				voiceIndex,
				vMirror.generation,
				soundHandle.GetUUID(),
				VoiceSourceType::Stream
			});
			vMirror.state = VoiceState::PendingLoadInactive;
			Logger::Log(LogLevel::Debug, "Engine", "Deferring stream playback for voice %d. Asset not yet loaded.",
				voiceIndex);

			pbkHandle = PlaybackHandle::Create(voiceIndex, vMirror.generation);
			return Result::Ok;
		}

		uint32_t streamIndex;
		if (!m_state->freeStreams->Pop(streamIndex)) {
			m_state->freeVoices->Push(voiceIndex);
			vMirror.Reset();

			Logger::Log(LogLevel::Error, "Engine", "Failed to create playback instance. Stream pool exhausted.");
			return Result::StreamPoolExhausted;
		}

		// Send I/O request to prepare stream
		m_state->streamPool[streamIndex].state.store(StreamState::Preparing, std::memory_order_release);
		IoStreamRequest req = IoStreamRequest::PrepareStream(streamIndex, sound->filepath);
		if (!m_state->ioStreamRequests->Push(req)) {
			m_state->freeVoices->Push(voiceIndex);
			vMirror.Reset();

			m_state->streamPool[streamIndex].state.store(StreamState::Free, std::memory_order_release);
			m_state->freeStreams->Push(streamIndex);

			Logger::Log(LogLevel::Error, "Engine", "Failed to create playback instance. IO Request queue full.");
			return Result::IoRequestQueueFull;
		}

		RtCommand cmd = RtCommand::PrepareVoiceStreaming(
			voiceIndex,
			vMirror.generation,
			streamIndex,
			sound->channels,
			sound->sampleRate
		);
		m_state->rtCommands->Enqueue(cmd);

		pbkHandle = PlaybackHandle::Create(voiceIndex, vMirror.generation);
		return Result::Ok;
	}

	Result Engine::Play(PlaybackHandle handle) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;

		uint32_t voiceIndex = handle.GetIndex();
		uint32_t generation = handle.GetGeneration();

		VoiceMirror& vMirror = m_state->voicePoolMirror[voiceIndex];
		if (vMirror.generation != generation) {
			return Result::ExpiredHandle;
		}

		// Deffered playback
		if (vMirror.state == VoiceState::PendingLoadInactive) {
			vMirror.state = VoiceState::PendingLoadPlaying;
			Logger::Log(LogLevel::Warning, "Engine",
				"Calling play on asset that is still loading. Will play when finished loading.");
			return Result::Ok;
		}

		// Check for double calls
		if (vMirror.state == VoiceState::Playing || vMirror.state == VoiceState::PendingLoadPlaying) {
			Logger::Log(LogLevel::Warning, "Engine", "Calling play on handle already set to play.");
			return Result::Ok;
		}

		if (vMirror.state != VoiceState::Inactive && vMirror.state != VoiceState::Paused) {
			return Result::PlaybackCorrupted;
		}

		RtCommand cmd = RtCommand::PlayVoice(voiceIndex, generation);
		m_state->rtCommands->Enqueue(cmd);
		vMirror.state = VoiceState::Playing;

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
					VoiceID stoppedVoice = {index, generation};

					// Resident
					for (auto it = m_state->pendingResidentUnloads.begin(); it != m_state->pendingResidentUnloads.end(); ) {
						auto& waitingList = it->voicesToStop;

						for (size_t i = 0; i < waitingList.size(); i++) {
							if (waitingList[i] == stoppedVoice) {
								waitingList[i] = waitingList.back();
								waitingList.pop_back();
								break;
							}
						}

						if (waitingList.empty()) {
							m_state->assetRegistry->FreeResidentSound(it->handle);
							Logger::Log(LogLevel::Debug, "Engine", "Unloaded resident sound with handle %d (delayed)",
								it->handle.GetUUID());
							it = m_state->pendingResidentUnloads.erase(it);
						}
						else it++;
					}

					// Stream
					for (auto it = m_state->pendingStreamUnloads.begin(); it != m_state->pendingStreamUnloads.end(); ) {
						auto& waitingList = it->voicesToStop;

						for (size_t i = 0; i < waitingList.size(); i++) {
							if (waitingList[i] == stoppedVoice) {
								waitingList[i] = waitingList.back();
								waitingList.pop_back();
								break;
							}
						}

						if (waitingList.empty()) {
							m_state->assetRegistry->FreeStreamSound(it->handle);
							Logger::Log(LogLevel::Debug, "Engine", "Unloaded stream sound with handle %d (delayed)",
								it->handle.GetUUID());
							it = m_state->pendingStreamUnloads.erase(it);
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
					if (it->assetUuid == ev.assetUuid && it->sourceType == ev.sourceType) {
						VoiceMirror& vMirror = m_state->voicePoolMirror[it->voiceIndex];

						if (vMirror.generation == it->voiceGeneration &&
							(vMirror.state == VoiceState::PendingLoadInactive || vMirror.state == VoiceState::PendingLoadPlaying)) {
							RtCommand cmd;

							if (it->sourceType == VoiceSourceType::Resident) {
								ResidentSoundHandle handle = ResidentSoundHandle::FromUUID(ev.assetUuid);
								ResidentSound* sound = m_state->assetRegistry->GetResidentSound(handle);

								cmd = RtCommand::PrepareVoiceResident(
									it->voiceIndex,
									it->voiceGeneration,
									sound->pcmData.data(),
									sound->totalFrames,
									sound->channels,
									sound->sampleRate
								);
								m_state->rtCommands->Enqueue(cmd);

								if (vMirror.state == VoiceState::PendingLoadPlaying) {
									vMirror.state = VoiceState::Playing;
									m_state->rtCommands->Enqueue(RtCommand::PlayVoice(it->voiceIndex, it->voiceGeneration));
								}
								else {
									vMirror.state = VoiceState::Inactive;
								}
							}
							else if (it->sourceType == VoiceSourceType::Stream) {
								StreamSoundHandle handle = StreamSoundHandle::FromUUID(ev.assetUuid);
								StreamSound* sound = m_state->assetRegistry->GetStreamSound(handle);

								// Allocate stream context
								uint32_t streamIndex;
								if (!m_state->freeStreams->Pop(streamIndex)) {
									m_state->freeVoices->Push(it->voiceIndex);
									vMirror.Reset();

									Logger::Log(LogLevel::Error, "Engine",
										"Aborting deferred playback. Stream pool exhausted.");
									it = m_state->pendingPlaybacks.erase(it);
									continue;
								}

								// Send I/O request to prepare stream
								m_state->streamPool[streamIndex].state.store(StreamState::Preparing, std::memory_order_release);
								IoStreamRequest req = IoStreamRequest::PrepareStream(streamIndex, sound->filepath);
								if (!m_state->ioStreamRequests->Push(req)) {
									m_state->freeVoices->Push(it->voiceIndex);
									vMirror.Reset();

									m_state->streamPool[streamIndex].state.store(StreamState::Free, std::memory_order_release);
									m_state->freeStreams->Push(streamIndex);

									Logger::Log(LogLevel::Error, "Engine",
										"Aborting deferred playback. IO Request queue full.");
									it = m_state->pendingPlaybacks.erase(it);
									continue;
								}

								RtCommand cmd = RtCommand::PrepareVoiceStreaming(
									it->voiceIndex,
									vMirror.generation,
									streamIndex,
									sound->channels,
									sound->sampleRate
								);
								m_state->rtCommands->Enqueue(cmd);

								if (vMirror.state == VoiceState::PendingLoadPlaying) {
									vMirror.state = VoiceState::Playing;
									m_state->rtCommands->Enqueue(RtCommand::PlayVoice(it->voiceIndex, it->voiceGeneration));
								}
								else {
									vMirror.state = VoiceState::Inactive;
								}
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
					if (it->assetUuid == ev.assetUuid && it->sourceType == ev.sourceType) {
						VoiceMirror& vMirror = m_state->voicePoolMirror[it->voiceIndex];

						if (vMirror.generation == it->voiceGeneration) {
							Logger::Log(LogLevel::Warning, "Engine",
								"Aborting deferred playback. Asset failed to load.");
							vMirror.Reset();
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