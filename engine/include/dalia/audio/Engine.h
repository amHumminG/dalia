#pragma once

#include "dalia/core/Result.h"
#include "dalia/core/LogLevel.h"

#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "dalia/audio/EffectControl.h"

namespace dalia {

	class StringID;
	struct EngineInternalState;


	struct EngineConfig {
		LogLevel logLevel = LogLevel::Warning;
		LogCallback logCallback = nullptr;

		uint32_t residentSoundCapacity = 256; // Maybe this should be higher?
		uint32_t streamSoundCapacity = 256;

		uint32_t voiceCapacity		= 128;
		uint32_t maxActiveVoices	= 64;
		uint32_t streamCapacity		= 32;
		uint32_t busCapacity		= 64;

		// Effects
		uint32_t biquadCapacity		= 32;

		size_t rtCommandQueueCapacity		= 1024;
		size_t rtEventQueueCapacity			= 1024;
		size_t ioStreamRequestQueueCapacity	= 256;
		size_t ioLoadRequestQueueCapacity	= 64;
		size_t ioLoadEventQueueCapacity		= 64;
	};

	class Engine {
	public:
		Engine();
		~Engine();

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		Result Init(const EngineConfig& config);
		Result Shutdown();

		void Update();

		// --- Loose File Functionality ---

		Result LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath,
			AssetLoadCallback callback = nullptr, uint32_t* outRequestId = nullptr);

		Result UnloadSound(SoundHandle soundHandle);

		// --- Bussing Manipulation ---

		Result CreateBus(const char* identifier, const char* parentIdentifier = "Master");

		Result DestroyBus(const char* identifier);

		Result RouteBus(const char* identifier, const char* parentIdentifier);

		Result SetBusVolumeDb(const char* identifier, float volumeDb);

		// --- Effects ---

		Result CreateBiquadFilter(EffectHandle& handle, BiquadFilterType type, const BiquadConfig& config);
		Result SetBiquadParams(EffectHandle handle, const BiquadConfig& config);

		Result AttachEffectToBus(EffectHandle effect, const char* busIdentifier, uint32_t effectSlot);

		Result DetachEffectFromBus(const char* busIdentifier, uint32_t effectSlot);

		Result DestroyEffect(EffectHandle effect);


		// --- Soundbank Functionality ---

		// Result LoadBank(BankHandle& handle, StringID pathId);
		// Result Unload(BankHandle handle);

		// Result PostEvent(EventHandle& handle, StringID);



		// --- Playback manipulation ---

		Result CreatePlayback(PlaybackHandle& playback, SoundHandle sound,
			AudioEventCallback callback = nullptr);

		Result RoutePlayback(PlaybackHandle playback, const char* busIdentifier);

		Result Play(PlaybackHandle playback);

		Result Pause(PlaybackHandle playback);

		Result Stop(PlaybackHandle playback);


	private:
		EngineInternalState* m_state = nullptr;

	};
}