#pragma once
#include <string>
#include <vector>

#include "Soundbank.h"


namespace  dalia {

    struct AudioData {
        std::string name;
        AudioFormat format;
        uint8_t channels;   // 1 Mono, 2 Stereo
        uint16_t sampleRate;
        std::vector<uint8_t> bytes;
    };

    class Transcoder {
    public:
        Transcoder(AudioFormat format);
        ~Transcoder() = default;

        AudioData Transcode(const char* path);

    private:
        AudioFormat m_Format;
    };
}
