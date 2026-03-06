#include "project/Project.h"
#include <filesystem>

namespace dalia::studio {

    SoundAsset* Project::ImportSound(const std::string& filePath) {
        auto asset = std::make_unique<SoundAsset>();
        asset->id = GenerateId();
        asset->filePath = filePath;

        // Give initial name based on filename
        asset->name = ExtractNameFromPath(filePath);

        m_assets.push_back(std::move(asset));
        return m_assets.back().get();
    }

    SoundAsset* Project::GetAsset(uint32_t assetId) {
        auto it = std::ranges::find_if(m_assets,
            [assetId](const auto& asset) { return asset->id == assetId; });
        if (it != m_assets.end()) return it->get();
        else return nullptr;
    }

    const std::vector<std::unique_ptr<SoundAsset>>& Project::GetAllAssets() const {
        return m_assets;
    }

    std::string Project::ExtractNameFromPath(const std::string& path) {
        auto filePath = std::filesystem::path(path);
        return filePath.filename().stem().generic_string();
    }

    uint32_t Project::GenerateId() {
        static uint32_t id = 0;
        return id++;
    }
}
