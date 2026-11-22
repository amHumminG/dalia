#pragma once
#include "dalia/Sound.hpp"
#include "core/ResourcePool.hpp"

// Asset types
#include "asset/types/ResidentSound.hpp"
#include "asset/types/StreamSound.hpp"

#include "asset/SoundDescriptor.hpp"

#include "core/Types.hpp"

#include <unordered_map>

namespace dalia {

	class AssetRegistry {
	public:
		AssetRegistry(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity);
		~AssetRegistry() = default;

		AssetRegistry(const AssetRegistry&)            = delete;
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
