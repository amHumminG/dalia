#pragma once

#include "dalia/Playback.hpp"
#include "dalia/Result.hpp"

#include "core/Constants.hpp"
#include "core/SPSCRingBufferPCM.hpp"
#include "core/StateSyncPool.hpp"
#include "core/Profiler.hpp"
#include "async/WakeableThread.hpp"

#include "mixer/processors/EffectProcessor.hpp"
#include "mixer/processors/AcousticProcessor.hpp"
#include "mixer/processors/VoiceProcessor.hpp"
#include "mixer/processors/BusProcessor.hpp"
#include "mixer/dsp/PeakLimiter.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>

namespace dalia {

	class AsyncMessageBus;

	struct MixerConfig {
		CoordinateSystem coordinateSystem = CoordinateSystem::LeftHanded;
		SpeakerLayout speakerLayout       = SpeakerLayout::Mono;
		uint32_t periodSizeFrames         = 512;
		uint32_t maxSamplesPeriod         = 0;
		uint32_t channels                 = CHANNELS_MONO;
		uint32_t sampleRate               = 48000;

		std::span<StreamContext> streamPool;
		MixerPoolView<Voice, VoiceParams> voicePool;
		MixerPoolView<Listener, ListenerParams> listenerPool;
		MixerPoolView<Bus, BusParams> busPool;
		MixerPoolView<Biquad, BiquadParams> biquadPool;
		MixerPoolView<FdnReverb, FdnReverbParams> fdnReverbPool;
	};

	class Mixer {
	public:
		Mixer();
		~Mixer();

		Result Initialize(AsyncMessageBus* asyncMessageBus, WakeableThread* mixerThread, const MixerConfig& config);
		void Shutdown();

		/// @brief Real-time thread entry point executed when mixerThread wakes up.
		void OnWake();

		// === OutputDevice / DeviceController Interface ===
		size_t ReadDiscardAudio(float* buffer, uint32_t frameCount);
		size_t DiscardAudio(uint32_t frameCount);
		void Wake();
		void SetOutputFormat(uint32_t channels, SpeakerLayout layout);

	private:
		void ProcessAudioPeriod();
		void SyncNodes();
		void Render(float* output, uint32_t frameCount);

		bool m_initialized = false;

		// --- Synchronization ---
		std::atomic<bool> m_isProcessing{false};
		std::atomic<bool> m_suspendRequest{false};

		AsyncMessageBus* m_asyncMessageBus = nullptr;
		WakeableThread* m_mixerThread      = nullptr;

		AcousticProcessor m_acousticProcessor;
		VoiceProcessor m_voiceProcessor;
		BusProcessor m_busProcessor;
		EffectProcessor m_effectProcessor;

		// --- Output ---
		uint32_t m_periodSizeFrames = 0;
		uint32_t m_maxSamplesPeriod = 0;
		uint32_t m_outChannels      = 0;
		uint32_t m_outSampleRate    = 0;
		float m_fadeStep            = 0.0f;
		float m_outputFadeGain      = 0.0f;

		PeakLimiter m_masterPeakLimiter;
		std::unique_ptr<SPSCRingBufferPCM<float>> m_pcmRingBuffer;
		std::unique_ptr<float[]> m_scratchBuffer;
	};
}
