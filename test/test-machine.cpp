#include <gtest/gtest.h>

#include "../src/fork.h"
#include "../src/platform.h"
#include <gmock/gmock.h>


// I want to mock this:
class testEnvironment : public platformEnvironment {
    // virtual
MOCK_METHOD(ReturnType, MethodName, (Args...));
};

class machineTest : public testing::Test {
protected:
    machineTest() {
        // q0_ remains empty
        // q1_.push_back(1);
        // q2_.Enqueue(2);
        // q2_.Enqueue(3);
    }

    // ~QueueTest() override = default;

    forkingMachine<Time, KeyCode> *fm;
    // Queue<int> q1_;
    // Queue<int> q2_;
};


//
TEST_F(machineTest, IsEmptyInitially) {
#if 0
    accept_event()

    expect call to
    virtual void hand_over_event_to_next_plugin(PlatformEvent* event);

#endif

    //EXPECT_EQ(q0_.size(), 0);
}

