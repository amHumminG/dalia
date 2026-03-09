#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <algorithm>

typedef struct stb_vorbis stb_vorbis;

namespace dalia {

    static constexpr size_t DOUBLE_BUFFER_FRAMES = 16384;
    static constexpr size_t MAX_CHANNELS = 2;
    static constexpr size_t DOUBLE_BUFFER_SIZE = DOUBLE_BUFFER_FRAMES * MAX_CHANNELS;

    static constexpr uint32_t NO_EOF = UINT32_MAX;

    enum class StreamState : uint8_t {
        Free,           // Not used by any voice
        Preparing,      // I/O thread is preparing the stream
        Streaming,      // Streaming from double buffer
        Finished        // Waiting to be freed by I/O thread
    };

    // Is this really thread safe?
    struct StreamingContext {
        std::atomic<StreamState> state = StreamState::Free;
        std::atomic<uint32_t> generation{0};

        alignas(64) float buffers[2][DOUBLE_BUFFER_SIZE];
        uint8_t frontBufferIndex = 0;
        int readCursor = 0;

        std::array<std::atomic<bool>, 2> bufferReady{false, false};
        std::array<uint32_t, 2> eofIndex = {NO_EOF, NO_EOF};

        stb_vorbis* decoder = nullptr;
        uint8_t channels = 0;
        uint32_t sampleRate = 0;

        // Called by the io-thread on request by the audio thread upon voice finish
        void Reset() {
            frontBufferIndex = 0;
            std::fill_n(&buffers[0][0], (2 * DOUBLE_BUFFER_SIZE), 0.0f);
            bufferReady[0].store(false);
            bufferReady[1].store(false);
            eofIndex = {NO_EOF, NO_EOF};

            readCursor = 0;

            // TODO: Make sure this resets everything to base values
        }
    };
}
