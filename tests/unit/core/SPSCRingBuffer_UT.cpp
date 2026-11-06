#include <gtest/gtest.h>
#include "core/SPSCRingBuffer.h"

#include <thread>

using namespace dalia;

TEST(SPSCRingBufferTest, CapacityRoundsToPowerOfTwo) {
	SPSCRingBuffer<int32_t> buffer(5);
	EXPECT_EQ(buffer.Capacity(), 8);
}

TEST(SpscRingBufferTest, PushPop) {
	SPSCRingBuffer<int32_t> buffer(4);

	int32_t val = 0;
	EXPECT_FALSE(buffer.Pop(val)); // Fails on empty

	EXPECT_TRUE(buffer.Push(42));
	EXPECT_TRUE(buffer.Pop(val));
	EXPECT_EQ(val, 42);

	EXPECT_FALSE(buffer.Pop(val));
}

TEST(SPSCRingBufferTest, BufferFullAndUsableCapacity) {
	SPSCRingBuffer<int32_t> buffer(4);

	EXPECT_TRUE(buffer.Push(1));
	EXPECT_TRUE(buffer.Push(2));
	EXPECT_TRUE(buffer.Push(3));
	EXPECT_FALSE(buffer.Push(4)); // This should fail due to masking

	int32_t val = 0;
	buffer.Pop(val); EXPECT_EQ(val, 1);
	buffer.Pop(val); EXPECT_EQ(val, 2);
	buffer.Pop(val); EXPECT_EQ(val, 3);
	EXPECT_FALSE(buffer.Pop(val));
}

TEST(SPSCRingBuffer, WrapAroundBehavior) {
	SPSCRingBuffer<int32_t> buffer(4);

	// Force cursors to wrap around
	for (int32_t i = 0; i < 20; i++) {
		EXPECT_TRUE(buffer.Push(i));

		int32_t val = -1;
		EXPECT_TRUE(buffer.Pop(val));
		EXPECT_EQ(val, i);
	}
}

TEST(SPSCRingBufferTest, PushOutputsWasEmpty) {
	SPSCRingBuffer<int32_t> buffer(8);
	bool wasEmpty = false;

	// Push to empty buffer
	EXPECT_TRUE(buffer.Push(10, wasEmpty));
	EXPECT_TRUE(wasEmpty);

	// Push to non-empty buffer
	EXPECT_TRUE(buffer.Push(10, wasEmpty));
	EXPECT_FALSE(wasEmpty);

	int32_t dummy;
	buffer.Pop(dummy);
	buffer.Pop(dummy);

	// Push to emtpy buffer (again)
	EXPECT_TRUE(buffer.Push(30, wasEmpty));
	EXPECT_TRUE(wasEmpty);
}

TEST(SPSCRingBufferTest, ConcurrencyStress) {
	SPSCRingBuffer<int32_t> buffer(1024);

	int32_t totalItems = 1000000;

	std::thread producer([&]() {
		for (int32_t i = 0; i < totalItems; i++) {
			while (!buffer.Push(i)) std::this_thread::yield();
		}
	});

	std::thread consumer([&]() {
		int32_t expected = 0;
		int32_t val = -1;

		while (expected < totalItems) {
			if (buffer.Pop(val)) {
				ASSERT_EQ(val, expected) << "Race condition detected." << expected;
				expected++;
			}
			else {
				std::this_thread::yield();
			}
		}
	});

	producer.join();
	consumer.join();
}