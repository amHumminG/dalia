#include "io/IoController.h"

#include "async/messaging/AsyncMEssageBus.h"
#include "async/messaging/AsyncLoadMessaging.h"
#include "async/messaging/AsyncStreamMessaging.h"
#include "core/Logger.h"

namespace dalia {

	IoController::IoController(uint32_t streamCapacity)
		: m_streams(streamCapacity) {
	}

	IoController::~IoController() {
		Shutdown();
	}

	Result IoController::Initialize(AsyncMessageBus* asyncMessageBus) {
		m_asyncMessageBus = asyncMessageBus;

		// Subscribe to load commands (Load Thread)
		m_asyncMessageBus->SubscribeToLoadCommand(AsyncLoadCommand::Type::LoadSound,
			[this](const AsyncLoadCommand& cmd) { HandleLoadSoundCommand(cmd); });

		// Subscribe to stream commands (Stream Thread)
		m_asyncMessageBus->SubscribeToStreamCommand(AsyncStreamCommand::Type::PrepareStream,
			[this](const AsyncStreamCommand& cmd) { HandlePrepareStreamCommand(cmd); });
		m_asyncMessageBus->SubscribeToStreamCommand(AsyncStreamCommand::Type::RefillStreamBuffer,
			[this](const AsyncStreamCommand& cmd) { HandleRefillStreamCommand(cmd); });
		m_asyncMessageBus->SubscribeToStreamCommand(AsyncStreamCommand::Type::SeekStream,
			[this](const AsyncStreamCommand& cmd) { HandleSeekStreamCommand(cmd); });
		m_asyncMessageBus->SubscribeToStreamCommand(AsyncStreamCommand::Type::ReleaseStream,
			[this](const AsyncStreamCommand& cmd) { HandleReleaseStreamCommand(cmd); });

		return Result::Ok;
	}

	void IoController::Shutdown() {
		// Unsubscribe from load commands (Load Thread)
		m_asyncMessageBus->UnsubscribeFromLoadCommand(AsyncLoadCommand::Type::LoadSound);

		// Unsubscribe from stream commands (Stream Thread)
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::PrepareStream);
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::RefillStreamBuffer);
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::SeekStream);
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::ReleaseStream);

		// Clean up any open stream decoders
		std::span<StreamContext> pool = m_streams.GetSpan();
		for (StreamContext& stream : pool) {
			if (stream.codec && stream.decoderHandle) stream.codec->CloseStream(stream.decoderHandle);
		}

		m_asyncMessageBus = nullptr;
	}

	Result IoController::AllocateAndPrepareStream(const char* filepath, uint32_t& streamIndex, uint32_t& streamGen) {
		if (!m_streams.Allocate(streamIndex)) return Result::StreamCapacityReached;

		StreamContext& stream = m_streams.Get(streamIndex);
		streamGen = stream.gen.load(std::memory_order_acquire);
		stream.state.store(StreamState::Preparing, std::memory_order_release);

		AsyncStreamCommand cmd = AsyncStreamCommand::PrepareStream(streamIndex, streamGen, filepath);
		if (m_asyncMessageBus->PushStreamCommand(cmd)) {
			// Push failed -> Rollback
			stream.state.store(StreamState::Free, std::memory_order_release);
			m_streams.Free(streamIndex);
			return Result::StreamQueueCapacityReached;
		}

		return Result::Ok;
	}

	std::span<StreamContext> IoController::GetStreamPoolSpan() {
		return m_streams.GetSpan();
	}

	void IoController::HandleLoadSoundCommand(const AsyncLoadCommand& cmd) {
		const Codec* codec = Codec::GetCodecForFile(cmd.filepath);
		if (!codec) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to load sound from file: %s. Unsupported file format.", cmd.filepath);
			m_asyncMessageBus->PushLoadEvent(
				AsyncLoadEvent::SoundLoadFailed(cmd.requestId, cmd.assetRawId, Result::UnsupportedFormat)
			);
			return;
		}

		AudioFormatInfo info{};
		Result res = Result::Ok;

		if (cmd.targetPcmBuffer != nullptr) {
			// Resident sound -> Decode full file into target pcm buffer
			res = codec->DecodeFile(cmd.filepath, *cmd.targetPcmBuffer, info);
		}
		else {
			// Stream sound -> Probe header metadata
			res = codec->ProbeFile(cmd.filepath, info);
		}

		if (res != Result::Ok) {
			m_asyncMessageBus->PushLoadEvent(
				AsyncLoadEvent(AsyncLoadEvent::SoundLoadFailed(cmd.requestId, cmd.assetRawId, res))
			);
			return;
		}

		m_asyncMessageBus->PushLoadEvent(AsyncLoadEvent::SoundLoaded(cmd.requestId, cmd.assetRawId, info));
		DALIA_LOG_DEBUG(LOG_CTX_ASYNC, "Loaded sound from file: %s.", cmd.filepath);
	}

	void IoController::HandlePrepareStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streams.Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;
		if (stream.state.load(std::memory_order_acquire) != StreamState::Preparing) return;

		if (stream.codec && stream.decoderHandle) stream.codec->CloseStream(stream.decoderHandle); // Safety check

		stream.codec = Codec::GetCodecForFile(cmd.data.streamPrep.filepath);
		if (!stream.codec) {
			stream.state.store(StreamState::Error, std::memory_order_release);
			return;
		}

		AudioFormatInfo info;
		if (stream.codec->OpenStream(cmd.data.streamPrep.filepath, stream.decoderHandle, info) == Result::Ok) {
			stream.channels = info.channels;
			stream.sampleRate = info.sampleRate;
			FillStreamBuffer(stream, 0);
			FillStreamBuffer(stream, 1);
			stream.state.store(StreamState::Streaming, std::memory_order_release);
			DALIA_LOG_DEBUG(LOG_CTX_ASYNC, "Prepared stream %u from file %s.", cmd.index, cmd.data.streamPrep.filepath);
		}
		else {
			stream.state.store(StreamState::Error, std::memory_order_release);
		}
	}

	void IoController::HandleRefillStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streams.Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;
		if (stream.state.load(std::memory_order_relaxed) != StreamState::Streaming) return;

		FillStreamBuffer(stream, cmd.data.streamRefill.bufferIndex);
	}

	void IoController::HandleSeekStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streams.Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;
		if (stream.state.load(std::memory_order_relaxed) != StreamState::Streaming) return;

		if (stream.codec && stream.decoderHandle) {
			stream.codec->SeekStreamFrame(stream.decoderHandle, cmd.data.streamSeek.seekFrame);
		}

		FillStreamBuffer(stream, 0);
		FillStreamBuffer(stream, 1);
		stream.state.store(StreamState::Streaming, std::memory_order_relaxed);
	}

	void IoController::HandleReleaseStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streams.Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;

		if (stream.codec && stream.decoderHandle) {
			stream.codec->CloseStream(stream.decoderHandle);
		}

		stream.Reset();
		m_streams.Free(cmd.index);
		DALIA_LOG_DEBUG(LOG_CTX_ASYNC, "Freed stream %d.", cmd.index);
	}

	void IoController::FillStreamBuffer(StreamContext& stream, uint32_t bufferIndex) {
		if (!stream.codec || !stream.decoderHandle) {
			stream.state.store(StreamState::Error, std::memory_order_release);
			return;
		}

		float* bufferPtr = stream.buffers[bufferIndex];
		uint32_t framesNeeded = STREAM_BUFFER_FRAMES;
		uint32_t framesWritten = 0;
		bool justLooped = false;
		bool foundEOF = false;

		while (framesWritten < framesNeeded) {
			uint32_t framesRemaining = framesNeeded - framesWritten;
			float* writePtr = bufferPtr + (framesWritten * stream.channels);

			uint32_t framesRead = stream.codec->DecodeStreamFrames(
				stream.decoderHandle, stream.channels, writePtr, framesRemaining
			);

			if (framesRead == 0) {
				// Hit EOF
				if (justLooped) {
					DALIA_LOG_ERR(LOG_CTX_ASYNC, "Stream loop failed. Corrupted file.");
					stream.state.store(StreamState::Error, std::memory_order_release);
					return;
				}

				stream.eofIndex[bufferIndex] = framesWritten;
				stream.codec->SeekStreamFrame(stream.decoderHandle, 0); // Loop
				DALIA_LOG_DEBUG(LOG_CTX_ASYNC, "Stream buffer found EOF at index %d.", framesWritten);
				justLooped = true;
				foundEOF = true;
				// Continue filling the buffer from the start of the file
			}
			else {
				if (!foundEOF) stream.eofIndex[bufferIndex] = NO_EOF;
				framesWritten += framesRead;
				justLooped = false;
			}
		}

		stream.bufferReady[bufferIndex].store(true, std::memory_order_release);
	}
}
