#include <gtest/gtest.h>
#include "async/IntervalThread.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace dalia;

TEST(IntervalThreadTest, LifecycleStartAndStop) {
	std::atomic<int32_t> tickCount{0};

	// Set a short interval (e.g., 5000 microseconds = 5ms)
	IntervalThread thread("TestInterval_Lifecycle", [&tickCount]() {
		tickCount++;
	}, 5000);

	thread.Start();

	// Let it run for a brief period to tick a few times
	std::this_thread::sleep_for(std::chrono::milliseconds(35));

	thread.Stop();

	int32_t recordedTicks = tickCount.load();
	// It should have ticked at least a couple of times
	EXPECT_GT(recordedTicks, 0);

	// Ensure that after stopping, the tick count stops incrementing
	int32_t ticksAtStop = recordedTicks;
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	EXPECT_EQ(tickCount.load(), ticksAtStop);
}

TEST(IntervalThreadTest, IntervalPacingSanityCheck) {
	std::atomic<int32_t> tickCount{0};

	// 20ms interval (20,000 microseconds)
	IntervalThread thread("TestInterval_Pacing", [&tickCount]() {
		tickCount++;
	}, 20000);

	thread.Start();

	// Run for ~70ms. At 20ms intervals, we expect roughly 3 to 4 ticks.
	std::this_thread::sleep_for(std::chrono::milliseconds(75));

	thread.Stop();

	int32_t count = tickCount.load();
	// Verify it's pacing reasonably (not firing thousands of times, nor zero times)
	EXPECT_GE(count, 2);
	EXPECT_LE(count, 6);
}
