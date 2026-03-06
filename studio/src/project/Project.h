#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <memory>

namespace dalia::studio {

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
    };

    class Project {
    public:
        Project() = default;
        ~Project() = default;

        SoundAsset* ImportSound(const std::string& filePath);
        SoundAsset* GetAsset(uint32_t assetId);

       const std::vector<std::unique_ptr<SoundAsset>>& GetAllAssets() const;

    private:
        static std::string ExtractNameFromPath(const std::string& path);
        static uint32_t GenerateId(); // TODO: Maybe change this to create a proper uuid

        std::vector<std::unique_ptr<SoundAsset>> m_assets;
    };
}