#include "asset/AssetCoordinator.h"

#include "asset/AssetRegistry.h"
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"
#include "async/messaging/AsyncMessageBus.h"
#include "core/AsyncPool.h"
#include "mixer/StreamContext.h"
#include "io/codec/Codec.h"
#include "core/Logger.h"

namespace dalia {

	Result AssetCoordinator::Initialize(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity,
		AsyncMessageBus* asyncMessageBus, AsyncPool<StreamContext>* streamPool) {

		m_assetRegistry = std::make_unique<AssetRegistry>(residentSoundCapacity, streamSoundCapacity);
		m_asyncMessageBus = asyncMessageBus;
		m_streamPool = streamPool;

		// Subscribe to load commands (Load Thread)
		m_asyncMessageBus->SubscribeToLoadCommand(AsyncLoadCommand::Type::LoadSound,
			[this](const AsyncLoadCommand& cmd) { HandleLoadSoundCommand(cmd); });

		// Subscribe to load events (Main Thread)
		m_asyncMessageBus->SubscribeToLoadEvent(AsyncLoadEvent::Type::SoundLoaded,
			[this](const AsyncLoadEvent& ev) { HandleSoundLoadedEvent(ev); });
		m_asyncMessageBus->SubscribeToLoadEvent(AsyncLoadEvent::Type::SoundLoadFailed,
			[this](const AsyncLoadEvent& ev) { HandleSoundLoadFailedEvent(ev); });

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

	void AssetCoordinator::Shutdown() {
		// Unsubscribe from load commands (Load Thread)
		m_asyncMessageBus->UnsubscribeFromLoadCommand(AsyncLoadCommand::Type::LoadSound);

		// Unsubscribe from load events (Main Thread)
		m_asyncMessageBus->UnsubscribeFromLoadEvent(AsyncLoadEvent::Type::SoundLoaded);
		m_asyncMessageBus->UnsubscribeFromLoadEvent(AsyncLoadEvent::Type::SoundLoadFailed);

		// Unsubscribe from stream commands (Stream Thread)
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::PrepareStream);
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::RefillStreamBuffer);
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::SeekStream);
		m_asyncMessageBus->UnsubscribeFromStreamCommand(AsyncStreamCommand::Type::ReleaseStream);
	}

	void AssetCoordinator::HandleLoadSoundCommand(const AsyncLoadCommand& cmd) {
		SoundHandle soundHandle = SoundHandle::FromRawId(cmd.data.soundFromFile.resourceHandleRawId);
		SoundType soundType = soundHandle.GetType();
		std::atomic<LoadState>* statePtr = nullptr;
		const char* filepath = cmd.data.soundFromFile.filepath;

		const Codec* codec = Codec::GetCodecForFile(filepath);
		if (!codec) {
			DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to load sound from file: %s. Unsupported file format.", filepath);
			m_asyncMessageBus->PushLoadEvent(AsyncLoadEvent::SoundLoadFailed(cmd.requestId, Result::UnsupportedFormat));
			return;
		}

		if (soundType == SoundType::Resident) {
			ResidentSound* sound = m_assetRegistry->GetResidentSound(soundHandle);
			if (!sound) return;
			statePtr = &sound->state;

			AudioFormatInfo info;
			Result res = codec->DecodeFile(filepath, sound->pcmData, info);
			if (res != Result::Ok) {
				// TODO: Remove this flagging. Communication should only go through the message bus here
				statePtr->store(LoadState::Error, std::memory_order_release);
				m_asyncMessageBus->PushLoadEvent(AsyncLoadEvent::SoundLoadFailed(cmd.requestId, res));
				return;
			}

			sound->channels = info.channels;
			sound->sampleRate = info.sampleRate;
			sound->frameCount = info.totalFrames;
			sound->lengthInSeconds = info.lengthInSeconds;
		}
		else if (soundType == SoundType::Stream) {
			StreamSound* sound = m_assetRegistry->GetStreamSound(soundHandle);
			if (!sound) return;
			statePtr = &sound->state;

			AudioFormatInfo info;
			Result res = codec->ProbeFile(filepath, info);
			if (res != Result::Ok) {
				statePtr->store(LoadState::Error, std::memory_order_release);
				m_asyncMessageBus->PushLoadEvent(AsyncLoadEvent::SoundLoadFailed(cmd.requestId, res));
				return;
			}

			sound->channels = info.channels;
			sound->sampleRate = info.sampleRate;
			sound->frameCount = info.totalFrames;
			sound->lengthInSeconds = info.lengthInSeconds;
			snprintf(sound->filepath, MAX_STR_LEN_ASSET_PATH, "%s", filepath);
		}

		statePtr->store(LoadState::Loaded, std::memory_order_release);
		m_asyncMessageBus->PushLoadEvent(AsyncLoadEvent::SoundLoaded(cmd.requestId, soundHandle.GetRawId()));
		DALIA_LOG_DEBUG(LOG_CTX_ASYNC, "Loaded sound from file: %s.", filepath);
	}

	void AssetCoordinator::HandlePrepareStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streamPool->Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;

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
		}
		else {
			stream.state.store(StreamState::Error, std::memory_order_release);
		}
	}

	void AssetCoordinator::HandleRefillStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streamPool->Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;
		if (stream.state.load(std::memory_order_relaxed) != StreamState::Streaming) return;

		FillStreamBuffer(stream, cmd.data.streamRefill.bufferIndex);
	}

	void AssetCoordinator::HandleSeekStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streamPool->Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;
		if (stream.state.load(std::memory_order_relaxed) != StreamState::Streaming) return;

		if (stream.codec && stream.decoderHandle) {
			stream.codec->SeekStreamFrame(stream.decoderHandle, cmd.data.streamSeek.seekFrame);
		}

		FillStreamBuffer(stream, 0);
		FillStreamBuffer(stream, 1);
		stream.state.store(StreamState::Streaming, std::memory_order_relaxed);
	}

	void AssetCoordinator::HandleReleaseStreamCommand(const AsyncStreamCommand& cmd) {
		StreamContext& stream = m_streamPool->Get(cmd.index);
		if (stream.gen.load(std::memory_order_relaxed) != cmd.gen) return;

		if (stream.codec && stream.decoderHandle) {
			stream.codec->CloseStream(stream.decoderHandle);
		}

		stream.Reset();
		m_streamPool->Free(cmd.index);
		DALIA_LOG_DEBUG(LOG_CTX_ASYNC, "Freed stream %d.", cmd.index);
	}

	void AssetCoordinator::FillStreamBuffer(StreamContext& stream, uint32_t bufferIndex) {
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
