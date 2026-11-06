#include <gtest/gtest.h>
#include "core/FixedStack.h"
#include <cstdint>

using namespace dalia;

TEST(FixedStackTest, InitialState) {
	FixedStack<int32_t> stack(10);

	EXPECT_EQ(stack.Capacity(), 10);
	EXPECT_EQ(stack.Size(), 0);
	EXPECT_TRUE(stack.IsEmpty());
	EXPECT_FALSE(stack.IsFull());
}

TEST(FixedStackTest, PushPop) {
	FixedStack<int32_t> stack(5);

	EXPECT_TRUE(stack.Push(10));
	EXPECT_TRUE(stack.Push(20));
	EXPECT_TRUE(stack.Push(30));
	EXPECT_EQ(stack.Size(), 3);

	int32_t val = 0;

	// Must pop in reverse order (Last-In, First-Out)
	EXPECT_TRUE(stack.Pop(val)); EXPECT_EQ(val, 30);
	EXPECT_TRUE(stack.Pop(val)); EXPECT_EQ(val, 20);
	EXPECT_TRUE(stack.Pop(val)); EXPECT_EQ(val, 10);

	EXPECT_TRUE(stack.IsEmpty());
}

TEST(FixedStackTest, BufferFull) {
	FixedStack<int32_t> stack(3);

	EXPECT_TRUE(stack.Push(1));
	EXPECT_TRUE(stack.Push(2));
	EXPECT_TRUE(stack.Push(3));

	EXPECT_TRUE(stack.IsFull());

	// 4th push must be rejected
	EXPECT_FALSE(stack.Push(4));
	EXPECT_EQ(stack.Size(), 3);
}

TEST(FixedStackTest, BufferEmpty) {
	FixedStack<int32_t> stack(3);
	int32_t val = 42;

	// Popping empty stack returns false and doesn't modify val
	EXPECT_FALSE(stack.Pop(val));
	EXPECT_EQ(val, 42);
}

TEST(FixedStackTest, ClearResetsState) {
	FixedStack<int32_t> stack(5);

	stack.Push(1);
	stack.Push(2);
	EXPECT_EQ(stack.Size(), 2);

	stack.Clear();
	EXPECT_TRUE(stack.IsEmpty());
	EXPECT_EQ(stack.Size(), 0);

	// Safe to push again
	EXPECT_TRUE(stack.Push(99));
	EXPECT_EQ(stack.Size(), 1);
}
