#include "asset/AssetRegistry.h"
#include "core/Types.h"
#include "core/Logger.h"

namespace dalia {

    AssetRegistry::AssetRegistry(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity)
        : m_residentSoundPool(residentSoundCapacity),
        m_streamSoundPool(streamSoundCapacity) {
    }

    SoundHandle AssetRegistry::AllocateSound(SoundType type) {
        uint32_t index, generation;

        if (type == SoundType::Resident) {
            if (m_residentSoundPool.Allocate(index, generation)) {
                return SoundHandle::Create(index, generation, SoundType::Resident);
            }
        }
        else if (type == SoundType::Stream) {
            if (m_streamSoundPool.Allocate(index, generation)) {
                return SoundHandle::Create(index, generation, SoundType::Stream);
            }
        }

        return INVALID_SOUND_HANDLE;
    }

    void AssetRegistry::FreeSound(SoundHandle handle) {
        if (!handle.IsValid()) {
            DALIA_LOG_WARN(LOG_CTX_CORE, "Attempting to free sound with invalid handle (%s).", handle.GetRawId());
        }

        if (handle.GetType() == SoundType::Resident) {
            m_residentSoundPool.Free(handle.GetIndex(), handle.GetGeneration());
        }
        else if (handle.GetType() == SoundType::Stream) {
            m_streamSoundPool.Free(handle.GetIndex(), handle.GetGeneration());
        }
    }

    Result AssetRegistry::GetSoundDescriptor(SoundHandle sound, SoundDescriptor& soundDesc) {
    	if (!sound.IsValid()) return Result::InvalidHandle;

    	if (sound.GetType() == SoundType::Resident) {
			ResidentSound* rs = m_residentSoundPool.Get(sound.GetIndex(), sound.GetGeneration());
    		soundDesc.soundType = SoundType::Resident;
    		soundDesc.loadState = rs->state;
    		soundDesc.channels = rs->channels;
    		soundDesc.sampleRate = rs->sampleRate;
    		soundDesc.totalFrames = rs->totalFrames;
    		soundDesc.lengthInSeconds = rs->lengthInSeconds;

    		soundDesc.pcmData = rs->pcmData.data();
    	}
    	else {
    		StreamSound* ss = m_streamSoundPool.Get(sound.GetIndex(), sound.GetGeneration());
    		soundDesc.soundType = SoundType::Resident;
    		soundDesc.loadState = ss->state;
    		soundDesc.channels = ss->channels;
    		soundDesc.sampleRate = ss->sampleRate;
    		soundDesc.totalFrames = ss->totalFrames;
    		soundDesc.lengthInSeconds = ss->lengthInSeconds;
    		soundDesc.filepath = ss->filepath;
    	}

    	return Result::Ok;
    }

    ResidentSound* AssetRegistry::GetResidentSound(SoundHandle handle) {
        if (handle.GetType() != SoundType::Resident) return nullptr;
        return m_residentSoundPool.Get(handle.GetIndex(), handle.GetGeneration());
    }

    StreamSound* AssetRegistry::GetStreamSound(SoundHandle handle) {
        if (handle.GetType() != SoundType::Stream) return nullptr;
        return m_streamSoundPool.Get(handle.GetIndex(), handle.GetGeneration());
    }

    bool AssetRegistry::GetLoadedSoundHandle(SoundID pathId, SoundHandle& handle) {
        auto it = m_loadedSounds.find(pathId);
        if (it != m_loadedSounds.end()) {
            handle = it->second;
            return true;
        }

        return false;
    }

    void AssetRegistry::RegisterLoadedSound(SoundID pathId, SoundHandle handle) {
        m_loadedSounds[pathId] = handle;
    }

    void AssetRegistry::UnregisterLoadedSound(SoundID pathId) {
        m_loadedSounds.erase(pathId);
    }
}
