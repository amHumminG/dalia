#include "resources/AssetRegistry.h"
#include "core/Types.h"

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

        return SoundHandle{};
    }

    void AssetRegistry::FreeSound(SoundHandle handle) {
        if (!handle.IsValid()) {
            Logger::Log(LogLevel::Warning, "Engine", "Attempting to free invalid sound handle.");
        }

        if (handle.GetType() == SoundType::Resident) {
            m_residentSoundPool.Free(handle.GetIndex(), handle.GetGeneration());
        }
        else if (handle.GetType() == SoundType::Stream) {
            m_streamSoundPool.Free(handle.GetIndex(), handle.GetGeneration());
        }
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
        std::lock_guard<std::mutex> lock(m_pathMutex);
        auto it = m_loadedSounds.find(pathId);
        if (it != m_loadedSounds.end()) {
            handle = it->second;
            return true;
        }

        return false;
    }

    void AssetRegistry::RegisterLoadedSound(SoundID pathId, SoundHandle handle) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_loadedSounds[pathId] = handle;
    }

    void AssetRegistry::UnregisterLoadedSound(SoundID pathId) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_loadedSounds.erase(pathId);
    }
}
