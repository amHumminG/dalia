#include "project/Project.h"
#include <filesystem>
#include <iostream>

#include "Transcoder.h"
#include "app/Application.h"

namespace dalia::studio {

    SoundAsset* Project::ImportSound(const std::string& filePath) {
        auto asset = std::make_unique<SoundAsset>();
        asset->id = GenerateId();

        // Give initial name based on filename
        asset->name = ExtractNameFromPath(filePath);

        //TODO: Transcode and move to cache
        std::filesystem::path cachePath = std::filesystem::current_path() / "cache" / m_name / (std::to_string(asset->id) + ".ogg");
        asset->filePath = cachePath.string();
        std::cout << asset->filePath << std::endl;

        asset->status = AssetStatus::Importing;

        SoundAsset* assetPtr = asset.get();
        m_assets.push_back(std::move(asset));

        uint32_t currentId = asset->id;
        Transcoder::TranscodeToOggAsync(filePath, cachePath, [this, currentId](bool success) {
            Application::Get().SubmitToMainThread([this, currentId, success]() {
                if (SoundAsset* safeAsset = GetAsset(currentId)) {
                    if (success) {
                        safeAsset->status = AssetStatus::Ready;
                    }else {
                        safeAsset->status = AssetStatus::Error;
                    }
                } else {
                    std::cout << "Asset " << currentId << " was deleted during transcode.\n";
                }
            });
        });

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

    void Project::RemoveAsset(uint32_t assetId) {
        auto it = std::ranges::find_if(m_assets,
        [assetId](const auto& asset) { return asset->id == assetId; });
        if (it != m_assets.end()) {
            m_assets.erase(it);
        }
    }

    std::string Project::ExtractNameFromPath(const std::string& path) {
        auto filePath = std::filesystem::path(path);
        return filePath.filename().stem().generic_string();
    }

    uint32_t Project::GenerateId() {
        static uint32_t id = 1;
        return id++;
    }
}
