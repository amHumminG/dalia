#include <gtest/gtest.h>
#include "core/AsyncPool.h"

#include <thread>
#include <cstdint>

using namespace dalia;

TEST(AsyncPoolTest, CompleteAllocation) {
    AsyncPool<int32_t> pool(4);
    EXPECT_EQ(pool.GetCapacity(), 4);
    
    uint32_t idx1, idx2, idx3, idx4, idx5;
    
    // Because we fixed the m_freeList(capacity + 1) bug, all 4 must succeed
    EXPECT_TRUE(pool.Allocate(idx1));
    EXPECT_TRUE(pool.Allocate(idx2));
    EXPECT_TRUE(pool.Allocate(idx3));
    EXPECT_TRUE(pool.Allocate(idx4));
    
    // 5th allocation must fail
    EXPECT_FALSE(pool.Allocate(idx5));
    
    // Ensure it gave us unique indices
    EXPECT_NE(idx1, idx2);
    EXPECT_NE(idx3, idx4);
}

TEST(AsyncPoolTest, FreeRestoresCapacity) {
    AsyncPool<int32_t> pool(2);
    uint32_t idx1, idx2;
    
    pool.Allocate(idx1);
    pool.Allocate(idx2);
    
    // Free them
    EXPECT_TRUE(pool.Free(idx1));
    EXPECT_TRUE(pool.Free(idx2));
    
    // Should be able to allocate 2 again
    EXPECT_TRUE(pool.Allocate(idx1));
    EXPECT_TRUE(pool.Allocate(idx2));
}

TEST(AsyncPoolTest, ReadWriteData) {
    AsyncPool<int32_t> pool(1);
    uint32_t idx;
    
    pool.Allocate(idx);
    
    pool.Get(idx) = 42;
    EXPECT_EQ(pool.Get(idx), 42);
    
    auto span = pool.GetSpan();
    EXPECT_EQ(span[idx], 42);
}

TEST(AsyncPoolTest, AsyncAllocationAndFree) {
    // Tests that one thread can allocate while another frees concurrently
    AsyncPool<int32_t> pool(1024);
    const int32_t iterations = 500'000;
    
    // We need a separate lock-free queue to pass the allocated indices to the consumer
    SPSCRingBuffer<uint32_t> handoverQueue(1024);
    
    std::thread allocator([&]() {
        for (int32_t i = 0; i < iterations; ++i) {
            uint32_t index;
            // Spin until we get a slot from the pool
            while (!pool.Allocate(index)) { std::this_thread::yield(); }
            
            // Pass the index to the free thread
            while (!handoverQueue.Push(index)) { std::this_thread::yield(); }
        }
    });
    
    std::thread freer([&]() {
        for (int32_t i = 0; i < iterations; ++i) {
            uint32_t index;
            // Get index from allocator
            while (!handoverQueue.Pop(index)) { std::this_thread::yield(); }
            
            // Return it to the pool
            pool.Free(index);
        }
    });
    
    allocator.join();
    freer.join();
    
    // If the atomic logic holds, the pool should have all its indices back
    uint32_t finalAlloc;
    uint32_t successCount = 0;
    while (pool.Allocate(finalAlloc)) {
        successCount++;
    }
    EXPECT_EQ(successCount, 1024);
}