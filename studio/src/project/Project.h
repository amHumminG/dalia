#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <string>

namespace dalia::studio {

    enum class AssetStatus : uint32_t {
        Importing = 1,
        Ready = 2,
        Error = 3
    };

    struct SoundAsset {
        uint32_t id;
        std::string name;
        std::string filePath;

        // Sound values
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        bool isLooping = false;

        // Editor-only data
        bool isDirty = false;
        AssetStatus status = AssetStatus::Importing;
    };

    class Project {
    public:
        Project() = default;
        ~Project() = default;

        std::string m_name;

        SoundAsset* ImportSound(const std::string& filePath);
        SoundAsset* GetAsset(uint32_t assetId);

        const std::vector<std::unique_ptr<SoundAsset>>& GetAllAssets() const;

        void RemoveAsset(uint32_t assetId);

    private:
        static std::string ExtractNameFromPath(const std::string& path);
        static uint32_t GenerateId(); // TODO: Maybe change this to create a proper uuid

        std::vector<std::unique_ptr<SoundAsset>> m_assets;
    };
}