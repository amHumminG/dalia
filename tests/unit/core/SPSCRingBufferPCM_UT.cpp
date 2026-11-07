#include <gtest/gtest.h>
#include "core/SPSCRingBufferPCM.h"

#include <vector>
#include <cstdint>

using namespace dalia;

class SPSCRingBufferPCMTest : public ::testing::Test {
protected:
    SPSCRingBufferPCM<float> buffer{1024};
};

TEST_F(SPSCRingBufferPCMTest, InitialFormatState) {
    // 100 frames * 2 channels = 200 samples.
    // Bit ceil of 100 is 128 logical frames.
    buffer.ClearAndSetFormat(100, 2);

    EXPECT_EQ(buffer.CapacityLogicalFrames(), 128);
    EXPECT_EQ(buffer.GetAvailableFramesRead(), 0);
    EXPECT_EQ(buffer.GetAvailableFramesWrite(), 128);
}

TEST_F(SPSCRingBufferPCMTest, SequentialPushAndPop) {
    buffer.ClearAndSetFormat(128, 1); // 128 frames, mono

    std::vector<float> inData(64, 1.0f);
    std::vector<float> outData(64, 0.0f);

    // Push 64 frames
    EXPECT_TRUE(buffer.PushFrames(inData.data(), 64));
    EXPECT_EQ(buffer.GetAvailableFramesRead(), 64);
    EXPECT_EQ(buffer.GetAvailableFramesWrite(), 64);

    // Pop 64 frames
    size_t popped = buffer.PopFrames(outData.data(), 64);
    EXPECT_EQ(popped, 64);
    EXPECT_EQ(outData[0], 1.0f);
    EXPECT_EQ(outData[63], 1.0f);

    EXPECT_EQ(buffer.GetAvailableFramesRead(), 0);
    EXPECT_EQ(buffer.GetAvailableFramesWrite(), 128);
}

TEST_F(SPSCRingBufferPCMTest, BufferWrapAroundTearGuard) {
    // This tests the exact boundary where a Push or Pop splits across the end of the ring buffer
    buffer.ClearAndSetFormat(128, 2); // 128 frames, stereo

    // Push 100 frames to advance the cursor near the end of the buffer
    std::vector<float> dummyData(200, 0.5f);
    EXPECT_TRUE(buffer.PushFrames(dummyData.data(), 100));

    // Pop the 100 frames to free up space (Cursors: Push=100, Pop=100)
    std::vector<float> trash(200);
    buffer.PopFrames(trash.data(), 100);

    // Push 50 frames.
    // Frames 0-27 will be written to the end of the array (indices 100-127).
    // Frames 28-49 will wrap around and be written to the start (indices 0-21).
    std::vector<float> wrapData(100);
    for (int i = 0; i < 100; ++i) wrapData[i] = static_cast<float>(i); // Distinct payload to verify order

    EXPECT_TRUE(buffer.PushFrames(wrapData.data(), 50));

    // Pop the 50 frames. It must execute the split read perfectly in order.
    std::vector<float> verifyData(100, -1.0f);
    size_t popped = buffer.PopFrames(verifyData.data(), 50);

    EXPECT_EQ(popped, 50);
    // Because it's stereo (2 channels), 50 frames = 100 samples
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(verifyData[i], wrapData[i]) << "Mismatch at sample index " << i;
    }
}

TEST_F(SPSCRingBufferPCMTest, DiscardFramesReleasesMemory) {
    buffer.ClearAndSetFormat(128, 1);
    std::vector<float> data(64, 1.0f);

    buffer.PushFrames(data.data(), 64);
    EXPECT_EQ(buffer.GetAvailableFramesRead(), 64);

    size_t discarded = buffer.DiscardFrames(32);
    EXPECT_EQ(discarded, 32);
    EXPECT_EQ(buffer.GetAvailableFramesRead(), 32); // 32 frames left
}

TEST_F(SPSCRingBufferPCMTest, ConcurrencyNoTearing) {
    // 256 logical frames, 2 channels = 512 physical samples
    buffer.ClearAndSetFormat(256, 2);

    // Push 1 million frames in 64-frame chunks.
    // 256 buffer / 64 chunk = 4 chunks capacity.
    // This forces the cursors to wrap around thousands of times per second.
    const size_t totalFrames = 1000000;
    const size_t chunkSize = 64;

    std::thread producer([this]() {
        std::vector<float> block(chunkSize * 2); // Stereo block
        size_t framesPushed = 0;

        while (framesPushed < totalFrames) {
            // Encode the exact global frame sequence into the audio data
            for (size_t i = 0; i < chunkSize; ++i) {
                float expectedVal = static_cast<float>(framesPushed + i);
                block[(i * 2) + 0] = expectedVal; // Left channel
                block[(i * 2) + 1] = expectedVal; // Right channel
            }

            // PushFrames is all-or-nothing. If false, buffer is full.
            while (!buffer.PushFrames(block.data(), chunkSize)) {
                std::this_thread::yield();
            }

            framesPushed += chunkSize;
        }
    });

    std::thread consumer([this]() {
        std::vector<float> readBlock(chunkSize * 2);
        size_t framesPopped = 0;

        while (framesPopped < totalFrames) {
            // PopFrames reads *up to* the requested amount
            size_t popped = buffer.PopFrames(readBlock.data(), chunkSize);

            if (popped > 0) {
                // Verify that every single sample perfectly matches the mathematical sequence
                for (size_t i = 0; i < popped; ++i) {
                    float expectedVal = static_cast<float>(framesPopped + i);

                    ASSERT_EQ(readBlock[(i * 2) + 0], expectedVal)
                        << "Memory tear on Left channel at frame " << (framesPopped + i);
                    ASSERT_EQ(readBlock[(i * 2) + 1], expectedVal)
                        << "Memory tear on Right channel at frame " << (framesPopped + i);
                }
                framesPopped += popped;
            } else {
                std::this_thread::yield(); // Buffer empty
            }
        }
    });

    producer.join();
    consumer.join();
}
