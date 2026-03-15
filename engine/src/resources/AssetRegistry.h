#pragma once
#include "dalia/audio/ResourceHandle.h"
#include "core/HandlePool.h"

// Resource types
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"
// #include "resources/EventDescriptor"
// #include "resources/BankRepresentation"

#include "StringId.h"
#include <unordered_map>
#include <mutex>

namespace dalia {

    class AssetRegistry {
    public:
        AssetRegistry(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity);
        ~AssetRegistry() = default;
        AssetRegistry(const AssetRegistry&) = delete;
        AssetRegistry& operator=(const AssetRegistry&) = delete;

        ResidentSoundHandle AllocateResident();
        void FreeResidentSound(ResidentSoundHandle handle);
        ResidentSound* GetResidentSound(ResidentSoundHandle handle);

        StreamSoundHandle AllocateStreamSound();
        void FreeStreamSound(StreamSoundHandle handle);
        StreamSound* GetStreamSound(StreamSoundHandle handle);

        bool GetLoadedResidentSoundHandle(StringID pathId, ResidentSoundHandle& handle);
        void RegisterLoadedResidentSound(StringID pathId, ResidentSoundHandle handle);
        void UnregisterLoadedResidentSound(StringID pathId);

        bool GetLoadedStreamSoundHandle(StringID pathId, StreamSoundHandle& handle);
        void RegisterLoadedStreamSound(StringID pathId, StreamSoundHandle handle);
        void UnregisterLoadedStreamSound(StringID pathId);

        // bool GetEventBlueprint(uint32_t eventHash, EventBlueprint& blueprint);
        // bool RegisterEvent(uint32_t eventHash, const EventBlueprint blueprint);
        // void ClearBankEvents(BankHandle handle);


    private:
        HandlePool<ResidentSound, ResidentSoundHandle> m_residentSoundPool;
        HandlePool<StreamSound, StreamSoundHandle> m_streamSoundPool;

        std::unordered_map<StringID, ResidentSoundHandle> m_loadedResidentAssets;
        std::unordered_map<StringID, StreamSoundHandle> m_loadedStreamAssets;
        std::mutex m_pathMutex;

        // std::unordered_map<StringID, EventBlueprint> m_events;
        // std::mutex m_eventMutex;
    };
}
