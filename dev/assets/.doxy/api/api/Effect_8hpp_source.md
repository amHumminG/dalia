

# File Effect.hpp

[**File List**](files.md) **>** [**dalia**](dir_7ee0a0312321b1cfc250dbf199abb419.md) **>** [**Effect.hpp**](Effect_8hpp.md)

[Go to the documentation of this file](Effect_8hpp.md)


```C++
#pragma once


#include <cstdint>

namespace dalia {
    enum class EffectType : uint8_t {
        None = 0,       
        Biquad = 1,     
        FdnReverb = 2   
    };

    struct EffectHandle {
    public:
        EffectHandle() = default;

        explicit EffectHandle(uint64_t rawId)
            : m_rawId(rawId) {
        }

        bool IsValid() const {
            return m_rawId != 0;
        }

        bool operator==(const EffectHandle& other) const {
            return m_rawId == other.m_rawId;
        }

        bool operator!=(const EffectHandle& other) const {
            return m_rawId != other.m_rawId;
        }

        uint64_t GetRawId() const {
            return m_rawId;
        }

        EffectType GetType() const {
            return static_cast<EffectType>(m_rawId >> 56);
        }

    private:
        friend class Engine;
        friend class MixController;
        friend class Mixer;
        friend class BusProcessor;
        friend class EffectProcessor;

        static EffectHandle Create(uint32_t index, uint32_t generation, EffectType type) {
            EffectHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.m_rawId = typeBits | generationBits | indexBits;
            return handle;
        }

        static EffectHandle FromRawId(uint64_t rawId) {
            EffectHandle handle;
            handle.m_rawId = rawId;
            return handle;
        }

        uint32_t GetIndex() const {
            return static_cast<uint32_t>(m_rawId & 0xFFFFFFFF);
        }

        uint32_t GetGeneration() const {
            return static_cast<uint32_t>((m_rawId >> 32) & 0xFFFFFF);
        }

        uint64_t m_rawId = 0;
    };

    constexpr EffectHandle INVALID_EFFECT_HANDLE{};

    struct BiquadParams {
        enum class Type : uint8_t {
            LowPass  = 0,   
            HighPass = 1,   
            BandPass = 2    
        } type = Type::LowPass;

        float frequency = 20000.0f; 
        float resonance = 0.707f;   

        void Sanitize();
    };

    struct FdnReverbParams {
        float decayTimeSeconds  = 4.0f;     
        float roomSize          = 0.6f;     
        float dampingHz         = 2500.0f;  
        float preDelayMs        = 20.0f;    
        float dryMix            = 1.0f;     
        float wetMix            = 0.35f;    
        float lowCutHz          = 80.0f;    
        float modulation        = 0.5f;     
        bool freeze             = false;    

        void Sanitize();
    };
}
```


