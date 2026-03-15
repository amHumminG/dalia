#include "systems/IoLoadSystem.h"

#include "core/Logger.h"
#include "messaging/IoLoadRequestQueue.h"
#include "messaging/IoLoadEventQueue.h"

#include "resources/AssetRegistry.h"
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include "core/Utility.h"

namespace dalia {

    IoLoadSystem::IoLoadSystem(const IoLoadSystemConfig& config)
        : m_ioLoadRequests(config.ioLoadRequests),
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
                // TODO: Check if maybe we should sleep for longer here
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Based on OS interrupt (Windows: 15.6ms)
            }
        }
    }

    void IoLoadSystem::ProcessRequest(const IoLoadRequest& req) {
        switch (req.type) {
            case IoLoadRequest::Type::LoadResidentSound: {
                ResidentSoundHandle targetHandle = ResidentSoundHandle::FromUUID(req.data.soundFromFile.resourceHandleUuid);
                ResidentSound* sound = m_assetRegistry->GetResidentSound(targetHandle);
                if (!sound) return;

                int error;
                stb_vorbis* decoder = stb_vorbis_open_filename(req.data.soundFromFile.filepath, &error, nullptr);
                if (!decoder) {
                    sound->state.store(LoadState::Error, std::memory_order_release);
                    m_ioLoadEvents->Push(IoLoadEvent(IoLoadEvent::SoundLoadFailed(req.requestId, Result::FileReadError)));
                    Logger::Log(LogLevel::Error, "IoLoadSystem", "Failed to load resident sound from %s. %s.",
                        req.data.soundFromFile.filepath, GetStbVorbisErrorString(error));
                    return;
                }

                stb_vorbis_info info = stb_vorbis_get_info(decoder);
                uint32_t totalSamplesPerChannel = stb_vorbis_stream_length_in_samples(decoder);
                uint32_t totalFloatsNeeded = totalSamplesPerChannel * info.channels;

                sound->pcmData.resize(totalFloatsNeeded);
                sound->channels = info.channels;
                sound->sampleRate = info.sample_rate;
                sound->totalFrames = totalFloatsNeeded / info.channels;

                // Decode file into sound data vector
                int floatsDecoded = stb_vorbis_get_samples_float_interleaved(
                    decoder,
                    info.channels,
                    sound->pcmData.data(),
                    static_cast<int>(totalFloatsNeeded)
                );
                stb_vorbis_close(decoder);

                if (floatsDecoded == 0) {
                    sound->state.store(LoadState::Error, std::memory_order_release);
                    m_ioLoadEvents->Push(IoLoadEvent(IoLoadEvent::SoundLoadFailed(req.requestId, Result::FileReadError)));
                    Logger::Log(LogLevel::Error, "IoLoadSystem", "Read %d samples from %s",
                        floatsDecoded, req.data.soundFromFile.filepath);
                    return;
                }

                sound->state.store(LoadState::Loaded, std::memory_order_release);
                m_ioLoadEvents->Push(IoLoadEvent(IoLoadEvent::SoundLoaded(req.requestId, targetHandle.GetUUID(),
                    VoiceSourceType::Resident)));
                Logger::Log(LogLevel::Debug, "IoLoadSystem", "Successfully loaded resident sound for file: %s",
                    req.data.soundFromFile.filepath);

                break;
            }
            case IoLoadRequest::Type::LoadStreamSound: {
                StreamSoundHandle targetHandle = StreamSoundHandle::FromUUID(req.data.soundFromFile.resourceHandleUuid);
                StreamSound* sound = m_assetRegistry->GetStreamSound(targetHandle);
                if (!sound) break;

                int error;
                stb_vorbis* decoder = stb_vorbis_open_filename(req.data.soundFromFile.filepath, &error, nullptr);
                if (!decoder) {
                    sound->state.store(LoadState::Error, std::memory_order_release);
                    m_ioLoadEvents->Push(IoLoadEvent(IoLoadEvent::SoundLoadFailed(req.requestId, Result::FileReadError)));
                    Logger::Log(LogLevel::Error, "IoLoadSystem", "Failed to load stream sound from %s. %s.",
                        req.data.soundFromFile.filepath, GetStbVorbisErrorString(error));
                    return;
                }

                strncpy_s(sound->filepath, MAX_IO_PATH_SIZE, req.data.soundFromFile.filepath, _TRUNCATE);

                stb_vorbis_info info = stb_vorbis_get_info(decoder);
                sound->channels = info.channels;
                sound->sampleRate = info.sample_rate;
                sound->totalFrames = stb_vorbis_stream_length_in_samples(decoder);

                stb_vorbis_close(decoder);

                sound->state.store(LoadState::Loaded, std::memory_order_release);
                m_ioLoadEvents->Push(IoLoadEvent(IoLoadEvent::SoundLoaded(req.requestId, targetHandle.GetUUID(),
                    VoiceSourceType::Stream)));
                Logger::Log(LogLevel::Debug, "IoLoadSystem", "Successfully loaded stream sound for file: %s",
                    req.data.soundFromFile.filepath);
                break;
            }
            default:
                break;
        }
    }
}
