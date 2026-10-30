#pragma once

#include "dalia/PlaybackControl.h"
#include "core/Constants.h"
#include "core/StateSyncPool.h"
#include "mixer/nodes/Listener.h"
#include "mixer/nodes/Voice.h"
#include "mixer/Speakers.h"

#include <cstdint>
#include <span>

namespace dalia {

	class AsyncMessageBus;
	struct AsyncMixerCommand;

	struct AcousticProcessorConfig {
		CoordinateSystem coordinateSystem	= CoordinateSystem::LeftHanded;
		SpeakerLayout speakerLayout			= SpeakerLayout::Mono;
		uint32_t outChannels				= CHANNELS_MONO;
		MixerPoolView<Listener, ListenerParams> listenerPool;
	};

	class AcousticProcessor {
	public:
		AcousticProcessor() = default;
		~AcousticProcessor();

		void Initialize(AsyncMessageBus* asyncMessageBus, const AcousticProcessorConfig& config);
		void Shutdown();

		void SyncNodes() { m_listenerPool.SyncParams(); }
		void SetOutputFormat(uint32_t outChannels, SpeakerLayout layout);

		void EvaluateVoices(std::span<Voice> voices);

	private:
		void SubscribeToCommands();
		void UnsubscribeFromCommands();

		void HandleSetGlobalDopplerFactorCommand(const AsyncMixerCommand& cmd);

		void ConfigureSpeakerLayout(SpeakerLayout layout);

		void EvaluateNonSpatialVoice(Voice& voice) const;
		void EvaluateSpatialVoice(Voice& voice) const;
		bool FindBestListener(const Voice& voice, ListenerParams& outBestListenerParams, float& outMaxGain) const;
		[[nodiscard]] float ComputeDopplerMultiplier(const Voice& voice, const ListenerParams& listenerParams) const;

		AsyncMessageBus* m_asyncMessageBus = nullptr;

		CoordinateSystem m_coordinateSystem = CoordinateSystem::LeftHanded;
		float m_globalDopplerFactor = DOPPLER_FACTOR_DEFAULT;

		uint32_t m_outChannels = CHANNELS_MONO;
		SpeakerLayout m_speakerLayout = SpeakerLayout::Mono;
		VirtualSpeaker m_speakerMatrix[CHANNELS_MAX]{};
		uint32_t m_spatialSpeakerCount = 0;

		MixerPoolView<Listener, ListenerParams> m_listenerPool;
	};
}
