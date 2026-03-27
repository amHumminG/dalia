#pragma once
#include <cstdint>

namespace dalia {

    //FNV-1 hash
    namespace Hash {
        constexpr uint32_t FNV1a_32(const char* str) {
            uint32_t hash = 0x811c9dc5;
            while (*str) {
                hash ^= static_cast<uint8_t>(*str);
                hash *= 0x01000193;
                ++str;
            }
            return hash;
        }
    }


    class StringID {
    public:
        constexpr StringID() : m_hash(0) {}
        // converts string to hash att compile time
        constexpr StringID(const char* str) : m_hash(Hash::FNV1a_32(str)) {}

        constexpr StringID(const uint32_t hash) : m_hash(hash) {}

        constexpr uint32_t GetHash() const { return m_hash; }

        constexpr bool operator==(const StringID& other) const {
            return m_hash == other.m_hash;
        }

    private:
        uint32_t m_hash;
    };
}

