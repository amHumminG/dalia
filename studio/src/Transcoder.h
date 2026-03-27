#pragma once
#include <string>
#include <vector>

#include "Soundbank.h"


namespace  dalia::studio {

    struct AudioData {
        std::string name;
        AudioFormat format;
        uint8_t channels;   // 1 Mono, 2 Stereo
        uint16_t sampleRate;
        std::vector<uint8_t> bytes;
    };

    //TODO: vet ej om detta är sättet man vill gör det på
    enum data_type {
        RAW_DATA = 0,
        FILE_DATA = 1
    };

    class Transcoder {
    public:
        Transcoder(AudioFormat format, data_type type);
        ~Transcoder() = default;

        AudioData Transcode(const char* path);

    private:
        AudioFormat m_Format;
    };
}
