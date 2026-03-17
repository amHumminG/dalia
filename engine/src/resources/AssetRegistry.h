#pragma once
// #include "dalia/audio/ResourceHandle.h"
#include "dalia/audio/SoundHandle.h"
#include "core/ResourcePool.h"

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

        SoundHandle AllocateSound(SoundType type);
        void FreeSound(SoundHandle handle);

        ResidentSound* GetResidentSound(SoundHandle handle);
        StreamSound* GetStreamSound(SoundHandle handle);

        bool GetLoadedSoundHandle(StringID pathId, SoundHandle& handle);
        void RegisterLoadedSound(StringID pathId, SoundHandle handle);
        void UnregisterLoadedSound(StringID pathId);

        // bool GetEventBlueprint(uint32_t eventHash, EventBlueprint& blueprint);
        // bool RegisterEvent(uint32_t eventHash, const EventBlueprint blueprint);
        // void ClearBankEvents(BankHandle handle);


    private:
        ResourcePool<ResidentSound> m_residentSoundPool;
        ResourcePool<StreamSound> m_streamSoundPool;

        std::unordered_map<StringID, SoundHandle> m_loadedSounds;
        std::mutex m_pathMutex;

        // std::unordered_map<StringID, EventBlueprint> m_events;
        // std::mutex m_eventMutex;
    };
}
