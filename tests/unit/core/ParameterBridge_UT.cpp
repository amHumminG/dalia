#include <gtest/gtest.h>
#include "core/ParameterBridge.h"

#include <thread>

struct TestParams {
	float volume = 1.0f;
	float pitch = 1.0f;
};

TEST(ParameterBridgeTest, InitialState) {
	ParameterBridge<TestParams> bridge;
	TestParams out;

	// Nothing has been pushed, so consume should return false
	EXPECT_FALSE(bridge.ConsumeUpdate(out));
}

TEST(ParameterBridgeTest, PushAndConsume) {
	ParameterBridge<TestParams> bridge;

	TestParams inParams{ 0.5f, 2.0f };
	bridge.PushUpdate(inParams);

	TestParams outParams;
	EXPECT_TRUE(bridge.ConsumeUpdate(outParams));
	EXPECT_EQ(outParams.volume, 0.5f);
	EXPECT_EQ(outParams.pitch, 2.0f);

	// Second consume immediately after should be false (not dirty)
	EXPECT_FALSE(bridge.ConsumeUpdate(outParams));
}

TEST(ParameterBridgeTest, OverwriteBeforeConsumeGetsLatest) {
	ParameterBridge<TestParams> bridge;

	TestParams first{ 0.1f, 1.0f };
	TestParams second{ 0.9f, 1.0f };

	// Producer pushes twice before consumer wakes up
	bridge.PushUpdate(first);
	bridge.PushUpdate(second);

	TestParams outParams;
	EXPECT_TRUE(bridge.ConsumeUpdate(outParams));

	// We should get the second payload, and it should be clean afterwards
	EXPECT_EQ(outParams.volume, 0.9f);
	EXPECT_FALSE(bridge.ConsumeUpdate(outParams));
}

struct TearTestingParams {
	uint64_t a = 0;
	uint64_t b = 0;
	uint64_t c = 0;
	uint64_t d = 0;
};

TEST(ParameterBridgeTest, ConcurrencyNoMemoryTearing) {
	ParameterBridge<TearTestingParams> bridge;
	const uint64_t iterations = 5000000;

	std::thread producer([&bridge]() {
		TearTestingParams params;
		for (uint64_t i = 1; i <= iterations; ++i) {
			params.a = i;
			params.b = i;
			params.c = i;
			params.d = i;
			bridge.PushUpdate(params);
		}
	});

	std::thread consumer([&bridge]() {
		TearTestingParams out;
		uint64_t lastSeen = 0;

		while (lastSeen < iterations) {
			if (bridge.ConsumeUpdate(out)) {
				// If barriers fail, these values will not match each other
				ASSERT_EQ(out.a, out.b) << "Memory tear detected between fields A and B";
				ASSERT_EQ(out.b, out.c) << "Memory tear detected between fields B and C";
				ASSERT_EQ(out.c, out.d) << "Memory tear detected between fields C and D";

				// Ensure time only moves forward
				ASSERT_GT(out.a, lastSeen) << "ABA problem: read an older update after a newer one.";
				lastSeen = out.a;
			} else {
				std::this_thread::yield();
			}
		}
	});

	producer.join();
	consumer.join();
}
