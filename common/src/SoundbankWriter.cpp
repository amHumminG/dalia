#include "common/SoundbankWriter.h"

namespace dalia {
    SoundbankWriter::SoundbankWriter(const char *path) {
        m_outFile.open(path, std::ios::binary);

        //Header
        m_header.identifier[0] = 'D'; // Dalia
        m_header.identifier[1] = 'S'; // Sound
        m_header.identifier[2] = 'B'; // Bank
        m_header.identifier[3] = 'K';
        m_header.version = 1;
        m_header.entryCount = 0;
        m_header.tocOffset = 0;

        m_outFile.write(reinterpret_cast<char*>(&m_header), sizeof(m_header));
    }

    void SoundbankWriter::AddSound(StringID name, std::vector<uint8_t>& bytes, AudioFormat format,uint8_t channels, uint16_t sampleRate) {
        // Build toc entry
        TOCEntry entry;
        entry.soundHash = name.GetHash();
        entry.channels = channels;
        entry.sampleRate = sampleRate;
        entry.format = format;
        entry.offset = m_outFile.tellp();
        entry.size = bytes.size();

        m_tocEntries.push_back(entry);

        //TODO: make it align to better borders should not impact other code when this changes me thinks
        m_outFile.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
    }

    void SoundbankWriter::CloseSoundbank() {
        m_header.tocOffset = m_outFile.tellp();
        m_header.entryCount = m_tocEntries.size();

        if (!m_tocEntries.empty()) {
            m_outFile.write(
                reinterpret_cast<const char*>(m_tocEntries.data()),
                m_tocEntries.size() * sizeof(TOCEntry)
            );
        }
        m_outFile.seekp(0, std::ios::beg);
        m_outFile.write(reinterpret_cast<char*>(&m_header), sizeof(m_header));
        m_outFile.close();
    }
}
