#include <gtest/gtest.h>
#include "dalia/Engine.h"
#include "dalia/Log.h"

using namespace dalia;

class EngineLifecycleTest : public ::testing::Test {
protected:
	EngineConfig GetHeadlessConfig() {
		EngineConfig config;

		config.logLevel = LogLevel::None;
		config.logCallback = nullptr;

		config.startupOutputDeviceId = "null"; // Render to void

		config.residentSoundCapacity = 16;
		config.streamSoundCapacity = 16;
		config.voiceCapacity = 16;
		config.busCapacity = 16;

		return config;
	}
};

TEST_F(EngineLifecycleTest, InitializeAndShutdown) {
	Engine engine;
	EngineConfig config = GetHeadlessConfig();

	Result res = engine.Initialize(config);
	EXPECT_EQ(res, Result::Ok);

	res = engine.Shutdown();
	EXPECT_EQ(res, Result::Ok);
}

TEST_F(EngineLifecycleTest, DoubleInitialize) {
	Engine engine;
	EngineConfig config = GetHeadlessConfig();

	Result res = engine.Initialize(config);
	ASSERT_EQ(res, dalia::Result::Ok);

	// Initializing an already running engine must fail safely
	Result res2 = engine.Initialize(config);
	EXPECT_EQ(res2, dalia::Result::AlreadyInitialized);

	engine.Shutdown();
}

TEST_F(EngineLifecycleTest, UninitializedShutdown) {
	Engine engine;

	// Attempting to shut down an engine that hasn't booted up
	Result res = engine.Shutdown();
	EXPECT_EQ(res, dalia::Result::NotInitialized);
}

TEST_F(EngineLifecycleTest, MultipleShutdownsAreSafe) {
	Engine engine;
	EngineConfig config = GetHeadlessConfig();

	engine.Initialize(config);

	// The first shutdown cleans up
	Result res1 = engine.Shutdown();
	EXPECT_EQ(res1, dalia::Result::Ok);

	// Subsequent shutdowns should be safe no-ops
	Result res2 = engine.Shutdown();
	EXPECT_EQ(res2, dalia::Result::NotInitialized);
}

TEST_F(EngineLifecycleTest, UpdateWhenUninitializedDoesNotCrash) {
	Engine engine;

	// Calling Update() on a dead engine should do nothing
	EXPECT_NO_FATAL_FAILURE({
		engine.Update();
	});
}
