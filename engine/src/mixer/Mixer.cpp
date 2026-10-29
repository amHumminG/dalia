#include "mixer/Mixer.h"
#include "mixer/dsp/Buffer.h"
#include "mixer/dsp/Gain.h"
#include "core/Logger.h"

#include "mixer/nodes/Voice.h"
#include "mixer/nodes/Listener.h"
#include "mixer/nodes/Bus.h"
#include "dalia/EffectControl.h"
#include "mixer/nodes/effects/Biquad.h"

#include "async/messaging/AsyncMessageBus.h"

namespace dalia {

	Mixer::Mixer() = default;

	Mixer::~Mixer() {
		Shutdown();
	}

	Result Mixer::Initialize(AsyncMessageBus* asyncMessageBus, WakeableThread* mixerThread,
		const MixerConfig& config) {
		m_asyncMessageBus = asyncMessageBus;
		m_mixerThread = mixerThread;

		m_coordinateSystem = config.coordinateSystem;
		m_speakerLayout = config.speakerLayout;
		m_maxSamplesPeriod = config.maxSamplesPeriod;
		m_outChannels = config.channels;
		m_outSampleRate = config.sampleRate;

		m_streamPool = config.streamPool;
		m_voicePool = config.voicePool;
		m_listenerPool = config.listenerPool;
		m_busPool = config.busPool;
		m_biquadPool = config.biquadPool;

		// Allocate DSP Working Memory
		m_pcmRingBuffer = std::make_unique<SPSCRingBufferPCM<float>>(
			m_periodSizeFrames * PCM_RING_BUFFER_FRAMES * CHANNELS_MAX);
		auto busCount = static_cast<uint32_t>(m_busPool.capacity);
		m_mixGraphCompiler = std::make_unique<MixGraphCompiler>(busCount);
		m_mixOrder = std::make_unique<uint32_t[]>(busCount);
		m_busBufferPool = std::make_unique<float[]>(busCount * m_maxSamplesPeriod);
		std::memset(m_busBufferPool.get(), 0.0f, busCount * m_maxSamplesPeriod);
		m_mixScratchBuffer = std::make_unique<float[]>(m_maxSamplesPeriod);
		std::memset(m_mixScratchBuffer.get(), 0.0f, m_maxSamplesPeriod);

		m_smoothingCoefficient = 1.0f - std::exp(-2.0f * PI * SMOOTHING_CUTOFF_HZ /
			static_cast<float>(m_outSampleRate));
		m_fadeStep = dsp::CalculateLinearFadeStep(FADE_TIME_GAIN, m_outSampleRate);

		ConfigureSpeakerLayout(m_speakerLayout);
		m_masterPeakLimiter.Initialize(static_cast<float>(m_outSampleRate));
		m_pcmRingBuffer->ClearAndSetFormat(m_periodSizeFrames * PCM_RING_BUFFER_FRAMES, m_outChannels);

		SubscribeToCommands();
		return Result::Ok;
	}

	void Mixer::Shutdown() {
		if (!m_asyncMessageBus) return;

		UnsubscribeFromCommands();
		m_asyncMessageBus = nullptr;
		m_mixerThread	  = nullptr;
	}

	void Mixer::ProcessAudioPeriod() {
		m_isProcessing.store(true, std::memory_order_seq_cst);

		if (m_suspendRequest.load(std::memory_order_seq_cst)) {
			m_isProcessing.store(false, std::memory_order_seq_cst);
			return;
		}

		while (m_pcmRingBuffer->GetAvailableFramesWrite() >= m_periodSizeFrames) {
			m_asyncMessageBus->ProcessMixerCommands();
			SyncParams();

			Render(m_mixScratchBuffer.get(), m_periodSizeFrames);
			m_pcmRingBuffer->PushFrames(m_mixScratchBuffer.get(), m_periodSizeFrames);
		}

		m_isProcessing.store(false, std::memory_order_seq_cst);
	}

	size_t Mixer::ReadDiscardAudio(float* buffer, uint32_t frameCount) {
		size_t framesRead = m_pcmRingBuffer->PopFrames(buffer, frameCount);

		if (framesRead > 0) {
			dsp::ApplyBlockFade(buffer, static_cast<uint32_t>(framesRead), m_outChannels,
				m_outputFadeGain, GAIN_DEFAULT, m_fadeStep);
		}

		return framesRead;
	}

	size_t Mixer::DiscardAudio(uint32_t frameCount) {
		return m_pcmRingBuffer->DiscardFrames(frameCount);
	}

	void Mixer::Wake() {
		m_mixerThread->Wake();
	}

	void Mixer::SetOutputFormat(uint32_t channels, SpeakerLayout layout) {
		m_suspendRequest.store(true, std::memory_order_seq_cst);

		// Wait for mixer to finish processing audio
		while (m_isProcessing.load(std::memory_order_seq_cst)) {
			std::this_thread::yield();
		}

		m_outChannels = channels;
		m_speakerLayout = layout;
		ConfigureSpeakerLayout(layout);

		m_pcmRingBuffer->ClearAndSetFormat(m_periodSizeFrames * PCM_RING_BUFFER_FRAMES, channels);

		m_outputFadeGain = GAIN_SILENCE;
		m_suspendRequest.store(false, std::memory_order_seq_cst);
	}

	void Mixer::SubscribeToCommands() {
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AllocateVoice,
			[this](const AsyncMixerCommand& cmd) { HandleAllocateVoiceCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FreeVoice,
			[this](const AsyncMixerCommand& cmd) { HandleFreeVoiceCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::PrepareVoiceResident,
			[this](const AsyncMixerCommand& cmd) { HandlePrepareVoiceResidentCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::PrepareVoiceStreaming,
			[this](const AsyncMixerCommand& cmd) { HandlePrepareVoiceStreamingCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::SeekVoice,
			[this](const AsyncMixerCommand& cmd) { HandleSeekVoiceCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::PlayVoice,
			[this](const AsyncMixerCommand& cmd) { HandlePlayVoiceCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::PauseVoice,
			[this](const AsyncMixerCommand& cmd) { HandlePauseVoiceCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::StopVoice,
			[this](const AsyncMixerCommand& cmd) { HandleStopVoiceCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::SetVoiceParent,
			[this](const AsyncMixerCommand& cmd) { HandleSetVoiceParentCommand(cmd); });

		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AllocateBus,
			[this](const AsyncMixerCommand& cmd) { HandleAllocateBusCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FreeBus,
			[this](const AsyncMixerCommand& cmd) { HandleFreeBusCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::SetBusParent,
			[this](const AsyncMixerCommand& cmd) { HandleSetBusParentCommand(cmd); });

		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AllocateEffect,
			[this](const AsyncMixerCommand& cmd) { HandleAllocateEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FreeEffect,
			[this](const AsyncMixerCommand& cmd) { HandleFreeEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::AttachEffect,
			[this](const AsyncMixerCommand& cmd) { HandleAttachEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::FadeDetachEffect,
			[this](const AsyncMixerCommand& cmd) { HandleFadeDetachEffectCommand(cmd); });
		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::ForceDetachEffect,
			[this](const AsyncMixerCommand& cmd) { HandleForceDetachEffectCommand(cmd); });

		m_asyncMessageBus->SubscribeToMixerCommand(AsyncMixerCommand::Type::SetGlobalDopplerFactor,
			[this](const AsyncMixerCommand& cmd) { HandleSetGlobalDopplerFactorCommand(cmd); });
	}

	void Mixer::UnsubscribeFromCommands() {
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AllocateVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FreeVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PrepareVoiceResident);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PrepareVoiceStreaming);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SeekVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PlayVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PauseVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::StopVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SetVoiceParent);

		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AllocateBus);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FreeBus);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SetBusParent);

		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AllocateEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FreeEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AttachEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FadeDetachEffect);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::ForceDetachEffect);

		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SetGlobalDopplerFactor);
	}

	void Mixer::HandleSetGlobalDopplerFactorCommand(const AsyncMixerCommand& cmd) {
		m_globalDopplerFactor = cmd.data.floatVal.value;
	}

	void Mixer::SyncParams() {
		m_voicePool.SyncNodes();
		m_listenerPool.SyncNodes();
		m_busPool.SyncNodes();
		m_biquadPool.SyncNodes();
	}

	void Mixer::Render(float* output, uint32_t frameCount) {
		const uint32_t sampleCount = frameCount * m_outChannels;

		// Memory initialization
		std::memset(m_busBufferPool.get(), 0, m_busPool.capacity * m_maxSamplesPeriod * sizeof(float));

		// State resolvers
		ResolveVoiceAcoustics();
		ResolveVoiceStates();
		m_isMixOrderDirty |= ResolveBusStates();

		// Graph compilation
		if (m_isMixOrderDirty) {
			if (!CompileMixGraph()) {
				std::memset(output, 0, sampleCount * sizeof(float));
				return;
			}
			m_isMixOrderDirty = false;
		}

		// Render passes
		RenderVoices(frameCount);
		RenderBuses(frameCount);

		// Output
		float* masterBuffer = GetBusBuffer(MASTER_BUS_INDEX);

		m_masterPeakLimiter.ProcessBuffer(masterBuffer, frameCount, m_outChannels); // Limit peaks

		for (uint32_t i = 0; i < sampleCount; i++) {
			masterBuffer[i] = std::clamp(masterBuffer[i], -1.0f, 1.0f);
		}

		std::copy_n(masterBuffer, sampleCount, output);
	}

	bool Mixer::CompileMixGraph() {
		m_mixOrderSize = m_mixGraphCompiler->Compile(m_busPool.nodes, std::span(m_mixOrder.get(), m_busPool.capacity));

		if (m_mixOrderSize == 0) {
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Mix graph cycle detected");
			return false;
		}

		DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Recompiled bus graph.");
		return true;
	}

	void Mixer::ConfigureSpeakerLayout(SpeakerLayout layout) {
		for (uint32_t c = 0; c < CHANNELS_MAX; c++) {
			m_speakerMatrix[c] = { math::Vector3(0, 0, 0), c };
		}

		if (layout == SpeakerLayout::Mono) {
			m_speakerMatrix[0] = { math::AngleToVector3(0.0f),  0 };
			m_spatialSpeakerCount = 1;
		}
		else if (layout == SpeakerLayout::Stereo) {
			m_speakerMatrix[0] = { math::AngleToVector3(-30.0f), 0 }; // Left
			m_speakerMatrix[1] = { math::AngleToVector3(30.0f), 1 };  // Right
			m_spatialSpeakerCount = 2;
		}
		else if (layout == SpeakerLayout::Surround51) {
			// TODO: Implement before supporting 5.1 surround sound
			// m_speakerMatrix[0] = { math::AngleToVector3(-30.0f), 0 }; // Left
			// m_speakerMatrix[1] = { math::AngleToVector3(30.0f), 1 }; // Right
			// m_speakerMatrix[2] = { math::AngleToVector3(0.0f), 2 }; // Center
			// // Channel 3 is LFE (Subwoofer should not be spatialized)
			// m_speakerMatrix[3] = { math::AngleToVector3(-110.0f), 4 }; // Surround Left
			// m_speakerMatrix[4] = { math::AngleToVector3(110.0f), 5 }; // Surround Right
			// m_spatialSpeakerCount = 5;
		}
		else if (layout == SpeakerLayout::Surround71) {
			// TODO: Implement before supporting 7.1 surround sound
		}
	}

	float* Mixer::GetBusBuffer(uint32_t busIndex) const {
		if (busIndex >= m_busPool.capacity) return nullptr;
		return m_busBufferPool.get() + (busIndex * m_maxSamplesPeriod);
	}
}
