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

		config.residentSoundCapacity = 16;
		config.streamSoundCapacity = 16;
		config.voiceCapacity = 16;
		config.busCapacity = 16;

		return config;
	}
};

TEST_F(EngineLifecycleTest, CleanInitializeAndShutdown) {
	Engine engine;
	EngineConfig config = GetHeadlessConfig();

	Result res = engine.Initialize(config);
	EXPECT_EQ(res, Result::Ok);

	res = engine.Shutdown();
	EXPECT_EQ(res, Result::Ok);
}
