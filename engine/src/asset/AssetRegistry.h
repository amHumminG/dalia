#pragma once
#include "dalia/SoundControl.h"
#include "core/ResourcePool.h"

// Asset types
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"

#include "asset/SoundDescriptor.h"

#include "core/Types.h"

#include <unordered_map>

namespace dalia {

    class AssetRegistry {
    public:
        AssetRegistry(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity);
        ~AssetRegistry() = default;

        AssetRegistry(const AssetRegistry&) = delete;
        AssetRegistry& operator=(const AssetRegistry&) = delete;

        SoundHandle AllocateSound(SoundType type);
        void FreeSound(SoundHandle handle);

    	Result GetSoundDescriptor(SoundHandle sound, SoundDescriptor& soundDesc);
        ResidentSound* GetResidentSound(SoundHandle handle);
        StreamSound* GetStreamSound(SoundHandle handle);

        bool GetLoadedSoundHandle(SoundID pathId, SoundHandle& handle);
        void RegisterLoadedSound(SoundID pathId, SoundHandle handle);
        void UnregisterLoadedSound(SoundID pathId);

    private:
        ResourcePool<ResidentSound> m_residentSoundPool;
        ResourcePool<StreamSound> m_streamSoundPool;

        std::unordered_map<SoundID, SoundHandle> m_loadedSounds;
    };
}
