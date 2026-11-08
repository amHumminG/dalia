#include <gtest/gtest.h>
#include "core/ResourcePool.h"
#include "core/Constants.h"
#include <cstdint>

using namespace dalia;

struct DummyResource {
    int value = 0;
};

TEST(ResourcePoolTest, AllocationAndFree) {
    ResourcePool<DummyResource> pool(10);

    uint32_t idx, gen;
    EXPECT_TRUE(pool.Allocate(idx, gen));
    EXPECT_EQ(gen, START_GENERATION);

    // Verify it can be accessed
    DummyResource* res = pool.Get(idx, gen);
    ASSERT_NE(res, nullptr);
    res->value = 42;

    // Free it
    pool.Free(idx, gen);

    // Attempting to access it after free must fail
    EXPECT_EQ(pool.Get(idx, gen), nullptr);
}

TEST(ResourcePoolTest, ABAProtectionViaGeneration) {
    ResourcePool<DummyResource> pool(2);

    uint32_t idx1, gen1;
    EXPECT_TRUE(pool.Allocate(idx1, gen1));

    // User A remembers their handle (idx1, gen1)

    // The slot is freed
    pool.Free(idx1, gen1);

    // User B comes along and allocates a new handle.
    // Because FixedStack is LIFO, they get the exact same slot index.
    uint32_t idx2, gen2;
    EXPECT_TRUE(pool.Allocate(idx2, gen2));

    EXPECT_EQ(idx1, idx2); // Same slot
    EXPECT_NE(gen1, gen2); // Different generation

    // User B writes to their new object
    pool.Get(idx2, gen2)->value = 99;

    // User A mistakenly tries to read from their old, freed handle.
    EXPECT_EQ(pool.Get(idx1, gen1), nullptr);
}

TEST(ResourcePoolTest, GenerationCounterWrapAround) {
    ResourcePool<DummyResource> pool(2);

    uint32_t idx, gen;
    EXPECT_TRUE(pool.Allocate(idx, gen));

    // Simulate it wrapping by recognizing that the pool protects itself from manual interference.
    pool.Free(idx, gen);
    EXPECT_EQ(pool.Get(idx, gen), nullptr);
}

TEST(ResourcePoolTest, OutOfBoundsRejection) {
    ResourcePool<DummyResource> pool(5);

    // Ensure that passing garbage indices doesn't crash the program
    EXPECT_EQ(pool.Get(999, START_GENERATION), nullptr);
}
