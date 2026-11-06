#include <gtest/gtest.h>
#include "async/WakeableThread.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace dalia;

TEST(WakeableThreadTest, StartAndStopWithoutWaking) {
    std::atomic<int32_t> executionCount{0};
    
    {
        WakeableThread thread("TestWakeable_Idle", [&executionCount]() {
            executionCount++;
        });
        
        thread.Start();
        // Give it a moment to boot up and enter its wait loop
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        // Destructor calls Stop() automatically, or we can call it explicitly
        thread.Stop();
    }
    
    // Without calling Wake(), the processing function should never have been invoked
    EXPECT_EQ(executionCount.load(), 0);
}

TEST(WakeableThreadTest, WakeTriggersProcessFunction) {
    std::atomic<int32_t> executionCount{0};
    
    WakeableThread thread("TestWakeable_Single", [&executionCount]() {
        executionCount++;
    });
    
    thread.Start();
    
    // Wake the thread once
    thread.Wake();
    
    // Poll with a timeout to wait for the thread to process the wake signal
    int32_t attempts = 0;
    while (executionCount.load() == 0 && attempts < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    
    EXPECT_EQ(executionCount.load(), 1);
    
    thread.Stop();
}

TEST(WakeableThreadTest, MultipleWakesRunMultipleTimes) {
    std::atomic<int32_t> executionCount{0};
    
    WakeableThread thread("TestWakeable_Multiple", [&executionCount]() {
        executionCount++;
    });
    
    thread.Start();
    
    // Wake multiple times
    const int32_t targetWakes = 5;
    for (int32_t i = 0; i < targetWakes; ++i) {
        thread.Wake();
    }
    
    // Wait for all executions to complete
    int32_t attempts = 0;
    while (executionCount.load() < targetWakes && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    
    EXPECT_GE(executionCount.load(), targetWakes);
    
    thread.Stop();
}