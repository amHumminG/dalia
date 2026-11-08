#include <gtest/gtest.h>
#include "core/FixedQueue.h"
#include <cstdint>

using namespace dalia;

TEST(FixedQueueTest, Initialization) {
	FixedQueue<int32_t> queue(5); // 5 rounds up to 8

	EXPECT_EQ(queue.Capacity(), 8);
	EXPECT_TRUE(queue.IsEmpty());
	EXPECT_FALSE(queue.IsFull());
	EXPECT_EQ(queue.Size(), 0);
}

TEST(FixedQueueTest, PushPop) {
	FixedQueue<int32_t> queue(4); // Capacity 4

	EXPECT_TRUE(queue.Push(10));
	EXPECT_TRUE(queue.Push(20));
	EXPECT_TRUE(queue.Push(30));

	int32_t val = 0;
	EXPECT_TRUE(queue.Pop(val)); EXPECT_EQ(val, 10);
	EXPECT_TRUE(queue.Pop(val)); EXPECT_EQ(val, 20);
	EXPECT_TRUE(queue.Pop(val)); EXPECT_EQ(val, 30);

	EXPECT_TRUE(queue.IsEmpty());
}

TEST(FixedQueueTest, WrapAroundAndFullState) {
	FixedQueue<int32_t> queue(4); // Capacity 4, Usable 3

	// Push and pop enough times to wrap the cursors past the end of the array
	for (int32_t i = 0; i < 10; ++i) {
		EXPECT_TRUE(queue.Push(i));
		int32_t val = -1;
		EXPECT_TRUE(queue.Pop(val));
		EXPECT_EQ(val, i);
	}

	// Now fill it up
	EXPECT_TRUE(queue.Push(100));
	EXPECT_TRUE(queue.Push(200));
	EXPECT_TRUE(queue.Push(300));

	EXPECT_TRUE(queue.IsFull());
	EXPECT_FALSE(queue.Push(400)); // 4th push must fail
}

TEST(FixedQueueTest, ClearResetsCursors) {
	FixedQueue<int32_t> queue(8);

	queue.Push(1);
	queue.Push(2);

	queue.Clear();
	EXPECT_TRUE(queue.IsEmpty());

	// Should be able to fill entirely after clear
	for (int32_t i = 0; i < 7; ++i) {
		EXPECT_TRUE(queue.Push(i));
	}
	EXPECT_TRUE(queue.IsFull());
}