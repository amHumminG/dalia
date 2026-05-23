#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace dalia::studio {

    class Transcoder {
    public:
        // Transcodes any supported audio file to Ogg Vorbis asynchronously.
        static void TranscodeToOggAsync(
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destinationPath,
            std::function<void(bool)> onComplete
        );

    private:
        static bool TranscodeInternal(const std::string& inPath, const std::string& outPath);
    };

}