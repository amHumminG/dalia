#include "mixer/Mixer.h"
#include "mixer/dsp/Buffer.h"
#include "mixer/dsp/Gain.h"
#include "core/Logger.h"
#include "core/Assert.h"
#include "core/Profiler.h"
#include "async/AsyncMessageBus.h"

namespace dalia {

	Mixer::~Mixer() {
		Shutdown();
	}

	Result Mixer::Initialize(AsyncMessageBus* asyncMessageBus, WakeableThread* mixerThread,
		const MixerConfig& config) {
		DALIA_PROFILE_SCOPE_N("Mixer::Initialize");

		m_asyncMessageBus  = asyncMessageBus;
		m_mixerThread      = mixerThread;
		m_periodSizeFrames = config.periodSizeFrames;
		m_maxSamplesPeriod = config.maxSamplesPeriod;
		m_outChannels      = config.channels;
		m_outSampleRate    = config.sampleRate;

		float smoothingCoefficient = 1.0f - std::exp(-2.0f * PI * SMOOTHING_CUTOFF_HZ /
			static_cast<float>(m_outSampleRate));
		m_fadeStep = dsp::CalculateLinearFadeStep(FADE_TIME_GAIN, m_outSampleRate);

		m_acousticProcessor.Initialize(m_asyncMessageBus, {
			.coordinateSystem	= config.coordinateSystem,
			.speakerLayout		= config.speakerLayout,
			.outChannels		= m_outChannels,
			.listenerPool		= config.listenerPool
		});

		m_voiceProcessor.Initialize(m_asyncMessageBus, &m_busProcessor, {
			.voicePool            = config.voicePool,
			.streamPool           = config.streamPool,
			.maxSamplesPeriod     = m_maxSamplesPeriod,
			.outChannels          = m_outChannels,
			.outSampleRate        = m_outSampleRate,
			.smoothingCoefficient = smoothingCoefficient,
			.fadeStep             = m_fadeStep
		});

		m_busProcessor.Initialize(m_asyncMessageBus, &m_effectProcessor, {
			.busPool              = config.busPool,
			.maxSamplesPeriod     = m_maxSamplesPeriod,
			.outChannels          = m_outChannels,
			.outSampleRate        = m_outSampleRate,
			.smoothingCoefficient = smoothingCoefficient,
			.fadeStep             = m_fadeStep
		});

		m_effectProcessor.Initialize(m_asyncMessageBus, {
			.biquadPool       = config.biquadPool,
			.maxSamplesPeriod = m_maxSamplesPeriod,
			.outChannels      = m_outChannels,
			.outSampleRate    = m_outSampleRate
		});

		m_masterPeakLimiter.Initialize(static_cast<float>(m_outSampleRate));
		m_pcmRingBuffer = std::make_unique<SPSCRingBufferPCM<float>>(
			m_periodSizeFrames * PCM_RING_BUFFER_FRAMES * CHANNELS_MAX
		);
		m_pcmRingBuffer->ClearAndSetFormat(m_periodSizeFrames * PCM_RING_BUFFER_FRAMES, m_outChannels);
		m_scratchBuffer = std::make_unique<float[]>(m_maxSamplesPeriod);

		m_initialized = true;
		return Result::Ok;
	}

	void Mixer::Shutdown() {
		DALIA_PROFILE_SCOPE_N("Mixer::Shutdown");

		if (!m_initialized) return;

		m_acousticProcessor.Shutdown();
		m_voiceProcessor.Shutdown();
		m_busProcessor.Shutdown();
		m_effectProcessor.Shutdown();

		m_asyncMessageBus = nullptr;
		m_mixerThread	  = nullptr;

		m_initialized = false;
	}

	void Mixer::OnWake() {
		DALIA_ASSERT(m_initialized, "Mixer used before initialized.");
		DALIA_PROFILE_SCOPE_N("Mixer::OnWake");

		m_isProcessing.store(true, std::memory_order_seq_cst);

		if (m_suspendRequest.load(std::memory_order_seq_cst)) {
			m_isProcessing.store(false, std::memory_order_seq_cst);
			return;
		}

		while (m_pcmRingBuffer->GetAvailableFramesWrite() >= m_periodSizeFrames) {
			ProcessAudioPeriod();
		}

		m_isProcessing.store(false, std::memory_order_seq_cst);
	}

	size_t Mixer::ReadDiscardAudio(float* buffer, uint32_t frameCount) {
		DALIA_ASSERT(m_initialized, "Mixer used before initialized.");
		DALIA_PROFILE_SCOPE_N("Mixer::ReadDiscardAudio");

		size_t framesRead = m_pcmRingBuffer->PopFrames(buffer, frameCount);

		if (framesRead > 0) {
			dsp::ApplyBlockFade(
				buffer,
				static_cast<uint32_t>(framesRead),
				m_outChannels,
				m_outputFadeGain,
				GAIN_DEFAULT,
				m_fadeStep
			);
		}

		return framesRead;
	}

	size_t Mixer::DiscardAudio(uint32_t frameCount) {
		DALIA_ASSERT(m_initialized, "Mixer used before initialized.");
		DALIA_PROFILE_SCOPE_N("Mixer::DiscardAudio");

		return m_pcmRingBuffer->DiscardFrames(frameCount);
	}

	void Mixer::Wake() {
		DALIA_ASSERT(m_initialized, "Mixer used before initialized.");
		if (m_mixerThread) m_mixerThread->Wake();
	}

	void Mixer::SetOutputFormat(uint32_t channels, SpeakerLayout layout) {
		DALIA_ASSERT(m_initialized, "Mixer used before initialized.");
		DALIA_PROFILE_SCOPE_N("Mixer::SetOutputFormat");

		m_suspendRequest.store(true, std::memory_order_seq_cst);

		// Wait for mixer to finish processing audio
		while (m_isProcessing.load(std::memory_order_seq_cst)) {
			std::this_thread::yield();
		}

		m_outChannels = channels;
		m_acousticProcessor.SetOutputFormat(channels, layout);
		m_voiceProcessor.SetOutputChannels(channels);
		m_busProcessor.SetOutputChannels(channels);
		m_effectProcessor.SetOutputChannels(channels);

		m_pcmRingBuffer->ClearAndSetFormat(m_periodSizeFrames * PCM_RING_BUFFER_FRAMES, channels);

		m_outputFadeGain = GAIN_SILENCE;
		m_suspendRequest.store(false, std::memory_order_seq_cst);
	}

	void Mixer::ProcessAudioPeriod() {
		DALIA_PROFILE_AUDIO_FRAME();
		DALIA_PROFILE_SCOPE_N("Mixer::ProcessAudioPeriod");
		m_asyncMessageBus->ProcessMixerCommands();
		SyncNodes();

		Render(m_scratchBuffer.get(), m_periodSizeFrames);
		m_pcmRingBuffer->PushFrames(m_scratchBuffer.get(), m_periodSizeFrames);
	}

	void Mixer::SyncNodes() {
		DALIA_PROFILE_SCOPE_N("Mixer::SyncNodes");

		m_acousticProcessor.SyncNodes();
		m_voiceProcessor.SyncNodes();
		m_busProcessor.SyncNodes();
		m_effectProcessor.SyncNodes();
	}

	void Mixer::Render(float* output, uint32_t frameCount) {
		DALIA_PROFILE_SCOPE_N("Mixer::Render");

		const uint32_t sampleCount = frameCount * m_outChannels;

		// Zero bus buffers
		m_busProcessor.ClearBuffers();

		// Evaluate 2D/3D acoustics & voice transitions
		m_acousticProcessor.EvaluateVoices(m_voiceProcessor.GetVoices());
		m_voiceProcessor.ResolveVoiceStates();

		// Resolve bus routing & recompile mix order if necessary
		if (!m_busProcessor.ResolveStatesAndCompileGraph()) {
			std::memset(output, 0, sampleCount * sizeof(float));
			return;
		}

		// Render active voices into their target buses
		m_voiceProcessor.RenderVoices(frameCount);

		// Render buses
		m_busProcessor.RenderBuses(frameCount);

		// Output
		float* masterBuffer = m_busProcessor.GetBusBuffer(MASTER_BUS_INDEX);
		m_masterPeakLimiter.ProcessBuffer(masterBuffer, frameCount, m_outChannels);

		for (uint32_t i = 0; i < sampleCount; i++) {
			output[i] = std::clamp(masterBuffer[i], -1.0f, 1.0f);
		}
	}
}
