#pragma once
#include <algorithm>
#include <fstream>
#include <iosfwd>
#include <vector>

#include "Soundbank.h"
#include "common/StringID.h"


namespace dalia {
    class SoundbankWriter {
    public:
        SoundbankWriter(const char* location);
        //TODO: fix it so it returns some result thing
        void AddSound(StringID name, std::vector<uint8_t>& bytes, AudioFormat format, uint8_t channels, uint16_t sampleRate);
        void CloseSoundbank();
    private:
        std::vector<TOCEntry> m_tocEntries;
        std::ofstream m_outFile;
        Header m_header;
    };
}
