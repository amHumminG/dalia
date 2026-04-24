#include "io/IoLoadSystem.h"

#include "core/Logger.h"
#include "core/Utility.h"

#include "messaging/IoLoadRequestQueue.h"
#include "messaging/IoLoadEventQueue.h"

#include "resources/AssetRegistry.h"
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace dalia {

    IoLoadSystem::IoLoadSystem(const IoLoadSystemConfig& config)
        : m_outSampleRate(config.outSampleRate),
		m_ioLoadRequests(config.ioLoadRequests),
        m_ioLoadEvents(config.ioLoadEvents),
        m_assetRegistry(config.assetRegistry) {
    }

    IoLoadSystem::~IoLoadSystem() {
        Stop();
    }

    void IoLoadSystem::Start() {
        if (m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(true, std::memory_order_release);
        m_thread = std::thread(&IoLoadSystem::ThreadMain, this);
    }

    void IoLoadSystem::Stop() {
        if (!m_isRunning.load(std::memory_order_relaxed)) return;

        m_isRunning.store(false, std::memory_order_release);
        if (m_thread.joinable()) m_thread.join();
    }

    void IoLoadSystem::ThreadMain() {
        while (m_isRunning.load(std::memory_order_relaxed)) {
            bool didWork = false;
            IoLoadRequest req;

            while (m_ioLoadRequests->Pop(req)) {
                didWork = true;
                ProcessRequest(req);
            }

            if (!didWork) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Based on OS interrupt (Windows: 15.6ms)
            }
        }
    }

    void IoLoadSystem::ProcessRequest(const IoLoadRequest& req) {
        switch (req.type) {
            case IoLoadRequest::Type::LoadSound: {
                SoundHandle soundHandle = SoundHandle::FromRawId(req.data.soundFromFile.resourceHandleRawId);
                SoundType soundType = soundHandle.GetType();

                ResidentSound* residentSound = nullptr;
                StreamSound* streamSound = nullptr;
                std::atomic<LoadState>* soundLoadStatePtr = nullptr;

                if (soundType == SoundType::Resident) {
                    residentSound = m_assetRegistry->GetResidentSound(soundHandle);
                    if (!residentSound) {
                        DALIA_LOG_ERR(LOG_CTX_IO, "Failed to load resident sound. Invalid handle (%d)",
                            soundHandle.GetRawId());
                        return;
                    }
                    soundLoadStatePtr = &residentSound->state;
                }
                else {
                    streamSound = m_assetRegistry->GetStreamSound(soundHandle);
                    if (!streamSound) {
                        DALIA_LOG_ERR(LOG_CTX_IO, "Failed to load stream sound. Invalid handle (%d)",
                            soundHandle.GetRawId());
                        return;
                    }
                    soundLoadStatePtr = &streamSound->state;
                }

                int error;
                stb_vorbis* decoder = stb_vorbis_open_filename(req.data.soundFromFile.filepath, &error, nullptr);
                if (!decoder) {
                    soundLoadStatePtr->store(LoadState::Error, std::memory_order_release);

                    m_ioLoadEvents->Push(IoLoadEvent::SoundLoadFailed(req.requestId, Result::FileReadError));
                    DALIA_LOG_ERR(LOG_CTX_IO, "Failed to load sound from %s. %s.", req.data.soundFromFile.filepath,
                        GetStbVorbisErrorString(error));

                    return;
                }

                stb_vorbis_info info = stb_vorbis_get_info(decoder);
                uint32_t totalFrames = stb_vorbis_stream_length_in_samples(decoder);

                if (soundType == SoundType::Resident) {
                    uint32_t totalSamples = totalFrames * info.channels;

                    residentSound->pcmData.resize(totalSamples);
                    residentSound->channels = info.channels;
                    residentSound->sampleRate = info.sample_rate;
                    residentSound->frameCount = totalFrames;

                    // Decode file into sound data vector
                    int framesDecoded = stb_vorbis_get_samples_float_interleaved(
                        decoder,
                        info.channels,
                        residentSound->pcmData.data(),
                        static_cast<int>(totalSamples)
                    );

                    if (framesDecoded == 0) {
                        soundLoadStatePtr->store(LoadState::Error, std::memory_order_release);

                        m_ioLoadEvents->Push(IoLoadEvent::SoundLoadFailed(req.requestId, Result::FileReadError));
                        DALIA_LOG_ERR(LOG_CTX_IO, "Failed to read samples from %s.", req.data.soundFromFile.filepath);

                        stb_vorbis_close(decoder);
                        return;
                    }

                	// Resize data container if it was not filled completely
                	if (framesDecoded != static_cast<int>(totalFrames)) {
                		residentSound->frameCount = totalFrames;
                		residentSound->pcmData.resize(framesDecoded * info.channels);
                	}
                }
                else if (soundType == SoundType::Stream) {
                    streamSound->channels = info.channels;
                    streamSound->sampleRate = info.sample_rate;
                    streamSound->frameCount = totalFrames;
                    strncpy_s(streamSound->filepath, MAX_IO_PATH_SIZE, req.data.soundFromFile.filepath, _TRUNCATE);
                }
                stb_vorbis_close(decoder);
                soundLoadStatePtr->store(LoadState::Loaded, std::memory_order_release);

                m_ioLoadEvents->Push(IoLoadEvent::SoundLoaded(req.requestId, soundHandle.GetRawId()));

                DALIA_LOG_DEBUG(LOG_CTX_IO, "Loaded sound from file (%s).", req.data.soundFromFile.filepath);
                break;
            }
            default:
                break;
        }
    }
}
