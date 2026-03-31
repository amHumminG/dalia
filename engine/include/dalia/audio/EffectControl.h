#pragma once
#include <cstdint>

// This file holds all API-facing types that are used to configure and control effects

namespace dalia {

    enum class EffectType : uint8_t {
        None = 0,
        Biquad,
        // More to come
    };

    struct EffectHandle {
    public:
        [[nodiscard]] bool IsValid() const { return uuid != 0; }
        bool operator==(const EffectHandle& other) const { return uuid == other.uuid; }
        bool operator!=(const EffectHandle& other) const { return uuid != other.uuid; }

        EffectType GetType() const { return static_cast<EffectType>(uuid >> 56); }
        uint32_t GetIndex() const { return static_cast<uint32_t>(uuid & 0xFFFFFFFF); }
        uint32_t GetGeneration() const { return static_cast<uint32_t>((uuid >> 32) & 0xFFFFFF); }

        uint64_t GetUUID() const { return uuid; }

    private:
        friend class Engine;
        friend class RtSystem;
        friend struct EngineInternalState;

        static EffectHandle Create(uint32_t index, uint32_t generation, EffectType type) {
            EffectHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.uuid = typeBits | generationBits | indexBits;
            return handle;
        }

        static EffectHandle FromUUID(uint64_t rawUuid) {
            EffectHandle handle;
            handle.uuid = rawUuid;
            return handle;
        }

        uint64_t uuid = 0;
    };

    constexpr EffectHandle InvalidEffectHandle{};



    // --- Biquad Filter ---
    enum class BiquadFilterType {
        LowPass,
        HighPass,
        BandPass
    };

    struct BiquadConfig {
        float frequency = 20000.0f; // Hz
        float resonance = 0.707f;           // Resonance
    };


}