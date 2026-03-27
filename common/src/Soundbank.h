#pragma once
#include <cstdint>

namespace dalia {

#pragma pack(push, 1)
    struct Header
    {
        char identifier[4];
        uint32_t version;
        uint32_t entryCount;
        uint32_t tocOffset; // Bytes from start of bank
    };
    enum class AudioFormat : uint8_t {
        PCM_16 = 0,

    };
    struct TOCEntry
    {
        //kanske borde vara nått annat än en char arry, kanske är smidigare om man kör nån hash grej?
        uint32_t soundHash;
        uint32_t offset; // Bytes from start of bank
        uint32_t size; // Size in bytes

        AudioFormat format;
        uint8_t channels;   // 1 Mono, 2 Stereo tror vi behöver det
        uint16_t sampleRate;
    };
#pragma pack(pop)
}
