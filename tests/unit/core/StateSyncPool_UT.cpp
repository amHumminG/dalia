#include <gtest/gtest.h>
#include "core/StateSyncPool.h"

#include <thread>
#include <atomic>
#include <vector>

using namespace dalia;

struct TestParams {
    uint32_t frequency = 1000;
    uint32_t qFactor = 1000;
};

struct TestMirror {
    TestParams params;
};

struct TestMixerItem {
    TestParams activeParams;

    // Required by MixerPoolView::SyncParams()
    void SetParams(const TestParams& p) {
        activeParams = p;
    }
};

class StateSyncPoolTest : public ::testing::Test {
protected:
    StateSyncPool<TestMixerItem, TestMirror, TestParams> pool{ 10 };
};

TEST_F(StateSyncPoolTest, AllocationAndFree) {
    EXPECT_EQ(pool.Capacity(), 10);

    uint32_t idx1, idx2;
    EXPECT_TRUE(pool.Allocate(idx1));
    EXPECT_TRUE(pool.Allocate(idx2));

    EXPECT_NE(idx1, idx2);

    pool.Free(idx1);

    // Because FixedStack is LIFO, the next allocation should reuse idx1
    uint32_t idx3;
    EXPECT_TRUE(pool.Allocate(idx3));
    EXPECT_EQ(idx3, idx1);
}

TEST_F(StateSyncPoolTest, EndToEndSyncCycle) {
    uint32_t idx;
    ASSERT_TRUE(pool.Allocate(idx));

    // Producer
    // Modify the mirror parameters using the write accessor (marks dirty)
    TestParams& mirrorParams = pool.GetMirrorParamsForWrite(idx);
    mirrorParams.frequency = 440;

    // Producer thread ticks the engine and pushes all dirty mirrors into the lock-free bridges
    pool.Sync();

    // Consumer
    // Pulls the lock-free data and updates the its nodes
    auto mixerView = pool.GetMixerView();
    mixerView.SyncParams();

    // Verify
    // The actual MixerItem should now have the updated parameter
    const TestMixerItem& mixerItem = pool.Get(idx);
    EXPECT_EQ(mixerItem.activeParams.frequency, 440);
}

TEST_F(StateSyncPoolTest, NoSyncWithoutMarkingDirty) {
    uint32_t idx;
    ASSERT_TRUE(pool.Allocate(idx));

    // Bypassing GetMirrorParamsForWrite and secretly modifying the mirror
    pool.GetMirror(idx).params.frequency = 500;

    // Producer ticks
    pool.Sync();

    // Consumer ticks
    auto mixerView = pool.GetMixerView();
    mixerView.SyncParams();

    // Because MarkDirty() wasn't called, the bridge shouldn't have fired
    // The item should still have the default frequency of 1000
    EXPECT_EQ(pool.Get(idx).activeParams.frequency, 1000);

    // Now manually mark it dirty and try again
    pool.MarkDirty(idx);
    pool.Sync();
    mixerView.SyncParams();

    // Now it should be updated
    EXPECT_EQ(pool.Get(idx).activeParams.frequency, 500);
}

TEST_F(StateSyncPoolTest, Concurrency) {
    // We instantiate a larger local pool specifically for the stress test
    StateSyncPool<TestMixerItem, TestMirror, TestParams> syncPool(64);

    // Allocate all 64 slots
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t idx;
        ASSERT_TRUE(syncPool.Allocate(idx));
        indices.push_back(idx);
    }

    std::atomic<bool> isEngineRunning{true};

    // --- Producer Thread ---
    std::thread producerThread([&syncPool, &indices, &isEngineRunning]() {
        uint32_t counter = 1;
        while (counter <= 1'000'000) {
            // Rapidly update 4 random items per tick
            for (int i = 0; i < 4; ++i) {
                uint32_t targetIdx = indices[counter % 64];
                TestParams& params = syncPool.GetMirrorParamsForWrite(targetIdx);

                // Write a synchronized pair
                params.frequency = counter;
                params.qFactor = counter;
            }

            // Push to the lock-free boundary
            syncPool.Sync();
            counter++;
        }
        isEngineRunning = false; // Stop the mixer thread
    });

    // --- Consumer Thread ---
    std::thread consumerThread([&syncPool, &isEngineRunning]() {
        auto mixerView = syncPool.GetMixerView();

        while (isEngineRunning.load(std::memory_order_acquire)) {
            // Pull from the lock-free boundary
            mixerView.SyncParams();

            // Verify DSP integrity
            for (size_t i = 0; i < mixerView.capacity; ++i) {
                const TestParams& current = mixerView.nodes[i].activeParams;
                // If freq and qFactor don't match, we tore a read across an update
                ASSERT_EQ(current.frequency, current.qFactor)
                    << "Mixer thread read corrupted data at node " << i;
            }
        }
    });

    producerThread.join();
    consumerThread.join();
}