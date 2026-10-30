#include "mixer/VoiceProcessor.h"
#include "mixer/BusProcessor.h"
#include "mixer/dsp/Buffer.h"
#include "mixer/dsp/Resampler.h"
#include "async/messaging/AsyncMessageBus.h"
#include "async/messaging/AsyncMixerMessaging.h"
#include "async/messaging/AsyncStreamMessaging.h"
#include "core/Logger.h"
#include "core/Math.h"

#include <algorithm>
#include <cmath>

#include "../../../build/release/_deps/raylib-src/src/external/miniaudio.h"

namespace dalia {

	VoiceProcessor::~VoiceProcessor() {
		Shutdown();
	}

	void VoiceProcessor::Initialize(AsyncMessageBus* asyncMessageBus, BusProcessor* busProcessor,
		const VoiceProcessorConfig& config) {

		m_asyncMessageBus      = asyncMessageBus;
		m_busProcessor         = busProcessor;
		m_voicePool            = config.voicePool;
		m_streamPool           = config.streamPool;
		m_maxSamplesPeriod     = config.maxSamplesPeriod;
		m_outChannels          = config.outChannels;
		m_outSampleRate        = config.outSampleRate;
		m_smoothingCoefficient = config.smoothingCoefficient;
		m_fadeStep             = config.fadeStep;

		m_scratchBuffer = std::make_unique<float[]>(m_maxSamplesPeriod);

		SubscribeToCommands();
	}

	void VoiceProcessor::Shutdown() {
		if (!m_asyncMessageBus) return;

		UnsubscribeFromCommands();
		m_asyncMessageBus = nullptr;
		m_busProcessor	  = nullptr;
	}

	void VoiceProcessor::ResolveStates() {
		for (uint32_t vIndex = 0; vIndex < m_voicePool.capacity; vIndex++) {
			Voice& voice = m_voicePool.nodes[vIndex];
			if (voice.currentState == VoiceState::Free) continue;

			if (ShouldFreeVoice(voice)) {
				FreeVoice(vIndex);
				continue;
			}

			bool requiresSilence = RequiresSilentTransition(voice);
			if (requiresSilence) voice.targetFadeGain = 0.0f;

			bool isSilent = math::NearlyEqual(voice.currentFadeGain, 0.0f, EPSILON_GAIN) ||
				voice.currentState != VoiceState::Playing;

			if (isSilent) ApplyTransitions(voice);

			if (requiresSilence && voice.currentState == VoiceState::Playing) voice.targetFadeGain = GAIN_DEFAULT;
		}
	}

	uint32_t VoiceProcessor::RenderVoices(uint32_t frameCount) {
		uint32_t voicesRendered = 0;

		for (uint32_t vIndex = 0; vIndex <= m_voicePool.capacity; vIndex++) {
			Voice& voice = m_voicePool.nodes[vIndex];
			if (voice.currentState != VoiceState::Playing) continue;

			voicesRendered++;
			RenderVoice(vIndex, voice, frameCount);
		}

		return voicesRendered;
	}

	void VoiceProcessor::SubscribeToCommands() {
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
	}

	void VoiceProcessor::UnsubscribeFromCommands() {
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::AllocateVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::FreeVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PrepareVoiceResident);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PrepareVoiceStreaming);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SeekVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PlayVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::PauseVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::StopVoice);
		m_asyncMessageBus->UnsubscribeFromMixerCommand(AsyncMixerCommand::Type::SetVoiceParent);
	}

	void VoiceProcessor::HandleAllocateVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];

		voice.gen			= cmd.targetGen;
		voice.currentState	= VoiceState::Inactive;
		voice.targetState	= VoiceState::Inactive;
	}

	void VoiceProcessor::HandleFreeVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen) return;

		voice.Reset();
	}

	void VoiceProcessor::HandlePrepareVoiceResidentCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		// Voice setup
		voice.soundType					= SoundType::Resident;
		voice.data.resident.pcmData		= cmd.data.prepResident.pcmData;
		voice.data.resident.frameCount	= cmd.data.prepResident.totalFrames;
		voice.channels					= cmd.data.prepResident.channels;
		voice.sampleRate				= cmd.data.prepResident.sampleRate;
	}

	void VoiceProcessor::HandlePrepareVoiceStreamingCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		// Voice setup
		voice.soundType							= SoundType::Stream;
		voice.data.stream.streamContextIndex	= cmd.data.prepStreaming.streamIndex;
		voice.data.stream.streamContextGen		= cmd.data.prepStreaming.streamGen;
		voice.data.stream.frontBufferIndex		= 0;
		voice.channels							= cmd.data.prepStreaming.channels;
		voice.sampleRate						= cmd.data.prepStreaming.sampleRate;
	}

	void VoiceProcessor::HandleSeekVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.hasPendingSeek	= true;
		voice.pendingSeekFrame	= cmd.data.seek.seekFrame;
	}

	void VoiceProcessor::HandlePlayVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetState = VoiceState::Playing;
	}

	void VoiceProcessor::HandlePauseVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetState = VoiceState::Paused;
	}

	void VoiceProcessor::HandleStopVoiceCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetState	= VoiceState::Stopped;
		voice.isExiting		= true;
	}

	void VoiceProcessor::HandleSetVoiceParentCommand(const AsyncMixerCommand& cmd) {
		Voice& voice = m_voicePool.nodes[cmd.targetIndex];
		if (voice.gen != cmd.targetGen || voice.isExiting) return;

		voice.targetBusIndex = cmd.data.setParent.parentIndex;
	}

	bool VoiceProcessor::ShouldFreeVoice(Voice& voice) {
		if (voice.currentState == VoiceState::Stopped) return true;

		if (voice.currentBusIndex == NO_PARENT) {
			voice.exitCondition = PlaybackExitCondition::ExplicitStop;
			return true;
		}
		return false;
	}

	bool VoiceProcessor::RequiresSilentTransition(const Voice& voice) {
		if (voice.currentState != voice.targetState && voice.targetState != VoiceState::Playing) return true;
		if (voice.currentBusIndex != voice.targetBusIndex) return true;
		if (voice.hasPendingSeek) return true;

		return false;
	}

	void VoiceProcessor::ApplyTransitions(Voice& voice) {
		// Routing
		if (voice.currentBusIndex != voice.targetBusIndex) voice.currentBusIndex = voice.targetBusIndex;

		// Seek
		if (voice.hasPendingSeek) ExecutePendingSeek(voice);

		// State
		// Waiting for I/O before state transition
		if (voice.soundType == SoundType::Stream) {
			StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
			if (stream.state.load(std::memory_order_acquire) != StreamState::Streaming) return;
		}

		// Commit to target state
		if (voice.currentState != voice.targetState) {
			if (voice.targetState != VoiceState::Stopped) voice.exitCondition = PlaybackExitCondition::ExplicitStop;
			voice.currentState = voice.targetState;
		}
	}

	void VoiceProcessor::ExecutePendingSeek(Voice& voice) {
		if (voice.soundType == SoundType::Resident) {
			voice.cursor = static_cast<double>(voice.pendingSeekFrame);
			voice.hasPendingSeek = false;
			voice.pendingSeekFrame = 0;
		}
		else if (voice.soundType == SoundType::Stream) {
			StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
			StreamState expectedState = StreamState::Streaming;

			if (stream.state.compare_exchange_strong(expectedState, StreamState::Seeking,
				std::memory_order_release)) {
				m_asyncMessageBus->PushStreamCommand(AsyncStreamCommand::SeekStream(
					voice.data.stream.streamContextIndex,
					stream.gen.load(std::memory_order_relaxed),
					voice.pendingSeekFrame
				));

				voice.cursor = 0.0;
				voice.data.stream.frontBufferIndex = 0;
				voice.hasPendingSeek = false;
				voice.pendingSeekFrame = 0;
			}
		}
	}

	void VoiceProcessor::FreeVoice(uint32_t voiceIndex) {
		Voice& voice = m_voicePool.nodes[voiceIndex];

		if (voice.soundType == SoundType::Stream) {
			m_asyncMessageBus->PushStreamCommand(AsyncStreamCommand::ReleaseStream(
				voice.data.stream.streamContextIndex, m_streamPool[voice.data.stream.streamContextIndex].gen
			));
		}

		if (voice.exitCondition == PlaybackExitCondition::NaturalEnd) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d finished naturally.", voiceIndex);
		}
		else if (voice.exitCondition == PlaybackExitCondition::ExplicitStop) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d stopped explicitly.", voiceIndex);
		}
		else if (voice.exitCondition == PlaybackExitCondition::Evicted) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d evicted.", voiceIndex);
		}
		else if (voice.exitCondition == PlaybackExitCondition::Error) {
			DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Voice %d stopped by error.", voiceIndex);
		}

		m_asyncMessageBus->PushMixerEvent(AsyncMixerEvent::VoiceStopped(
			voiceIndex, voice.gen, voice.exitCondition
		));
		voice.Reset();
	}

	void VoiceProcessor::RenderVoice(uint32_t voiceIndex, Voice& voice, uint32_t frameCount) {
		float* busBuffer = m_busProcessor->GetBusBuffer(voice.currentBusIndex);
		if (!busBuffer) {
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Failed to get bus buffer for voice (index: %u).", voiceIndex);
			return;
		}

		uint32_t framesMixed = 0;
		float phaseInc = voice.finalPlaybackRate *
			(static_cast<float>(voice.sampleRate) / static_cast<float>(m_outSampleRate));

		while (framesMixed < frameCount) {
			uint32_t outputFramesNeeded = frameCount - framesMixed;
			uint32_t sourceFramesNeeded = static_cast<uint32_t>(
				std::ceil(static_cast<float>(outputFramesNeeded) * phaseInc)
			) + HERMITE_LOOKAHEAD_FRAMES;

			SourceBlock block = FetchSourceBlock(voice, sourceFramesNeeded);
			if (block.isError) {
				voice.currentState = VoiceState::Stopped;
				voice.exitCondition = PlaybackExitCondition::Error;
				break;
			}
			if (block.isUnderrun) {
				DALIA_LOG_DEBUG(LOG_CTX_MIXER, "Buffer underrun for voice (index: %u).", voiceIndex);
				break;
			}

			if (block.framesAvailable > 0) {
				uint32_t outputFramesGenerated = 0;
				uint32_t sourceFramesConsumed = 0;

				ProcessResampler(
					voice.resamplerState,
					block.pcmData,
					block.framesAvailable,
					block.channels,
					m_scratchBuffer.get(),
					outputFramesNeeded,
					phaseInc,
					outputFramesGenerated,
					sourceFramesConsumed
				);

				dsp::MixVoiceBlock(
					m_scratchBuffer.get(),
					&busBuffer[framesMixed * m_outChannels],
					outputFramesGenerated,
					block.channels,
					m_outChannels,
					voice.currentFadeGain,
					voice.targetFadeGain,
					m_fadeStep,
					voice.currentGainMatrix,
					voice.targetGainMatrix,
					m_smoothingCoefficient
				);

				voice.cursor += static_cast<double>(sourceFramesConsumed);
				framesMixed += outputFramesGenerated;
			}

			if (block.framesAvailable < sourceFramesNeeded) {
				if (!AdvanceVoiceBoundary(voice)) break;
			}
		}
	}

	VoiceProcessor::SourceBlock VoiceProcessor::FetchSourceBlock(const Voice& voice, uint32_t maxFramesNeeded) const {
		SourceBlock block;
		auto cursorInt = static_cast<uint32_t>(voice.cursor);

		if (voice.soundType == SoundType::Resident) {
			block.channels = voice.channels;
			uint32_t totalSourceFrames = voice.data.resident.frameCount;

			if (cursorInt < totalSourceFrames) {
				block.framesAvailable = std::min(maxFramesNeeded, totalSourceFrames - cursorInt);
				block.pcmData = voice.data.resident.pcmData + (cursorInt * block.channels);
			}

			return block;
		}

		if (voice.soundType == SoundType::Stream) {
			StreamContext& stream = m_streamPool[voice.data.stream.streamContextIndex];
			uint32_t frontBufIndex = voice.data.stream.frontBufferIndex;

			if (stream.state.load(std::memory_order_acquire) == StreamState::Error) {
				block.isError = true;
				return block;
			}

			if (!stream.bufferReady[frontBufIndex].load(std::memory_order_acquire)) {
				block.isUnderrun = true;
				return block;
			}

			block.channels = stream.channels;
			uint32_t framesInSource = STREAM_BUFFER_FRAMES;
			const uint32_t eofIndex = stream.eofIndex[frontBufIndex];

			if (!voice.params.isLooping && eofIndex != NO_EOF) framesInSource = eofIndex;

			if (cursorInt < framesInSource) {
				block.framesAvailable = std::min(maxFramesNeeded, framesInSource - cursorInt);
				block.pcmData = stream.buffers[frontBufIndex] + (cursorInt * block.channels);
			}

			return block;
		}

		block.isError = true;
		return block;
	}

	bool VoiceProcessor::AdvanceVoiceBoundary(Voice& voice) {
		if (voice.soundType == SoundType::Resident) {
			if (voice.params.isLooping) {
				voice.cursor -= static_cast<double>(voice.data.resident.frameCount);
				if (voice.cursor < 0.0) voice.cursor = 0.0;
				return true;
			}
			voice.currentState = VoiceState::Stopped;
			voice.exitCondition = PlaybackExitCondition::NaturalEnd;
			return false;
		}

		if (voice.soundType == SoundType::Stream) {
			uint32_t sIndex = voice.data.stream.streamContextIndex;
			uint32_t frontBufIndex = voice.data.stream.frontBufferIndex;
			StreamContext& stream = m_streamPool[sIndex];

			// Exit check
			if (!voice.params.isLooping && stream.eofIndex[frontBufIndex] != NO_EOF) {
				voice.currentState = VoiceState::Stopped;
				voice.exitCondition = PlaybackExitCondition::NaturalEnd;
				return false;
			}

			// No exit -> Swap front buffer
			stream.bufferReady[frontBufIndex].store(false, std::memory_order_release);
			m_asyncMessageBus->PushStreamCommand(AsyncStreamCommand::RefillStreamBuffer(
				sIndex,
				stream.gen.load(std::memory_order_relaxed),
				frontBufIndex
			));

			voice.data.stream.frontBufferIndex = 1 - frontBufIndex;
			voice.cursor -= static_cast<double>(STREAM_BUFFER_FRAMES);
			if (voice.cursor < 0.0) voice.cursor = 0.0;

			return true;
		}

		return false;
	}
}
