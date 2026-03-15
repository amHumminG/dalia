#include "resources/AssetRegistry.h"

namespace dalia {

    AssetRegistry::AssetRegistry(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity)
        : m_residentSoundPool(residentSoundCapacity),
        m_streamSoundPool(streamSoundCapacity) {
    }

    ResidentSoundHandle AssetRegistry::AllocateResident() {
        return m_residentSoundPool.Allocate();
    }

    void AssetRegistry::FreeResidentSound(ResidentSoundHandle handle) {
        m_residentSoundPool.Free(handle);
    }

    ResidentSound* AssetRegistry::GetResidentSound(ResidentSoundHandle handle) {
        return m_residentSoundPool.Get(handle);
    }

    StreamSoundHandle AssetRegistry::AllocateStreamSound() {
        return m_streamSoundPool.Allocate();
    }

    void AssetRegistry::FreeStreamSound(StreamSoundHandle handle) {
        m_streamSoundPool.Free(handle);
    }

    StreamSound* AssetRegistry::GetStreamSound(StreamSoundHandle handle) {
        return m_streamSoundPool.Get(handle);
    }

    bool AssetRegistry::GetLoadedResidentSoundHandle(StringID pathId, ResidentSoundHandle& handle) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        auto it = m_loadedResidentAssets.find(pathId);
        if (it != m_loadedResidentAssets.end()) {
            handle = it->second;
            return true;
        }

        return false;
    }

    void AssetRegistry::RegisterLoadedResidentSound(StringID pathId, ResidentSoundHandle handle) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_loadedResidentAssets[pathId] = handle;
    }

    void AssetRegistry::UnregisterLoadedResidentSound(StringID pathId) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_loadedResidentAssets.erase(pathId);
    }

    bool AssetRegistry::GetLoadedStreamSoundHandle(StringID pathId, StreamSoundHandle& handle) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        auto it = m_loadedStreamAssets.find(pathId);
        if (it != m_loadedStreamAssets.end()) {
            handle = it->second;
            return true;
        }

        return false;
    }

    void AssetRegistry::RegisterLoadedStreamSound(StringID pathId, StreamSoundHandle handle) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_loadedStreamAssets[pathId] = handle;
    }

    void AssetRegistry::UnregisterLoadedStreamSound(StringID pathId) {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_loadedStreamAssets.erase(pathId);
    }
}
