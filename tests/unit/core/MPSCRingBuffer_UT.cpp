#include <gtest/gtest.h>
#include "core/MPSCRingBuffer.h"

#include <thread>
#include <vector>
#include <cstdint>

using namespace dalia;

TEST(MPSCRingBufferTest, CapacityRoundsToPowerOfTwo) {
    // Capacity = std::bit_ceil(capacity + 1).
    // If we request 4, capacity + 1 is 5. bit_ceil(5) is 8.
    MPSCRingBuffer<int32_t> buffer(4);

    // Since capacity is private, we prove it indirectly by filling the buffer
    // A buffer size of 8 has 7 usable slots due to ring buffer empty/full math.
    for (int32_t i = 0; i < 7; ++i) {
        EXPECT_TRUE(buffer.Push(i));
    }
    EXPECT_FALSE(buffer.Push(99)); // 8th push should fail
}

TEST(MPSCRingBufferTest, PushPop) {
    MPSCRingBuffer<int32_t> buffer(4);

    int32_t val = 0;
    EXPECT_FALSE(buffer.Pop(val)); // Empty

    EXPECT_TRUE(buffer.Push(42));
    EXPECT_TRUE(buffer.Pop(val));
    EXPECT_EQ(val, 42);
}

TEST(MPSCRingBufferTest, WrapAround) {
    MPSCRingBuffer<int32_t> buffer(4);

    // Push and pop 50 times to force internal cursor wrap-around
    for (int32_t i = 0; i < 50; ++i) {
        EXPECT_TRUE(buffer.Push(i));
        int32_t val = -1;
        EXPECT_TRUE(buffer.Pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(MPSCRingBufferTest, MultiProducerStress) {
    MPSCRingBuffer<uint32_t> buffer(1024);

	uint32_t numProducers = 4;
    uint32_t itemsPerProducer = 100000;
    uint32_t totalItems = numProducers * itemsPerProducer;

    std::vector<std::thread> producers;

    // Spawn 4 Producer Threads
    for (uint32_t pId = 0; pId < numProducers; ++pId) {
        producers.emplace_back([&buffer, pId, itemsPerProducer]() {
            for (uint32_t i = 0; i < itemsPerProducer; ++i) {
                // Pack Producer ID in the top 8 bits, and the local counter in the bottom 24 bits
                uint32_t payload = (pId << 24) | (i & 0xFFFFFF);

                while (!buffer.Push(payload)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Spawn 1 Consumer Thread
    std::thread consumer([&buffer, numProducers, totalItems]() {
        uint32_t itemsPopped = 0;

        // Track the expected next sequence number for each producer
        std::vector<uint32_t> expectedSequence(numProducers, 0);

        uint32_t payload = 0;
        while (itemsPopped < totalItems) {
            if (buffer.Pop(payload)) {
                // Unpack the data
                uint32_t pId = payload >> 24;
                uint32_t sequence = payload & 0xFFFFFF;

                // Assert that THIS specific producer's FIFO order is perfectly intact
                ASSERT_LT(pId, numProducers) << "Corrupted payload detected.";
                ASSERT_EQ(sequence, expectedSequence[pId]) << "Race condition detected for Producer " << pId;

                expectedSequence[pId]++;
                itemsPopped++;
            } else {
                // Pop fails if buffer is empty OR if a producer claimed a slot but hasn't set 'ready' yet
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();
}
