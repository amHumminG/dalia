#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <algorithm>

typedef struct stb_vorbis stb_vorbis;

namespace dalia {

    static constexpr size_t DOUBLE_BUFFER_FRAMES = 16384;
    static constexpr size_t DOUBLE_BUFFER_SIZE = DOUBLE_BUFFER_FRAMES * CHANNELS_MAX;

    static constexpr uint32_t NO_EOF = UINT32_MAX;

    enum class StreamState : uint8_t {
        Free,           // Not used by any voice
        Preparing,      // I/O thread is preparing the stream
        Streaming,      // Streaming from double buffer
        Seeking,        // I/O thread is performing a seek and filling both buffers with new data
        Finished,       // Waiting to be freed by I/O thread
        Error           // Stream state is corrupted (something failed, probably on the I/O thread)
    };

    struct StreamContext {
        std::atomic<uint32_t> gen{0};
        std::atomic<StreamState> state = StreamState::Free;

        alignas(64) float buffers[2][DOUBLE_BUFFER_SIZE];
        int readCursor = 0;

        std::array<std::atomic<bool>, 2> bufferReady{false, false};
        std::array<uint32_t, 2> eofIndex = {NO_EOF, NO_EOF};

        stb_vorbis* decoder = nullptr;
        uint32_t channels = 0;
        uint32_t sampleRate = 0;

        // Called by the io-thread on request by the audio thread upon voice finish
        void Reset() {
            gen.fetch_add(1, std::memory_order_release);
            if (gen.load(std::memory_order_acquire) == NO_GENERATION) {
                gen.store(START_GENERATION, std::memory_order_release);
            }
            state.store(StreamState::Free, std::memory_order_release);

            std::fill_n(&buffers[0][0], (2 * DOUBLE_BUFFER_SIZE), 0.0f);
            readCursor = 0;

            bufferReady[0].store(false);
            bufferReady[1].store(false);
            eofIndex = {NO_EOF, NO_EOF};

            decoder = nullptr;
            channels = 0;
            sampleRate = 0;
        }
    };
}
