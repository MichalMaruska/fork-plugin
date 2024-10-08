#include <gtest/gtest.h>

#include "../src/queue.h"
#include "../src/platform.h"

class queueTest : public testing::Test {
protected:
    queueTest():
    q0_("test"),
    q1_("events")
    {
    };

    my_queue<int> q0_;
    my_queue<key_event> q1_;

    };


TEST_F(queueTest, IsEmptyInitially) {
    EXPECT_EQ(q0_.length(), 0);
    EXPECT_TRUE(q0_.empty());
    EXPECT_STREQ("test", q0_.get_name());
}

// push & test
TEST_F(queueTest, oneElement) {
    constexpr int new_value = 8;
    constexpr int old_value = 7;
    auto elem = old_value;

    q0_.push(elem);
    elem = new_value;
    EXPECT_EQ(q0_.length(), 1);
    EXPECT_EQ(q0_.front(), old_value);
}

// no, this is  FrontReference
TEST_F(queueTest, oneElementPointer) {
    int x = 7;
    int new_value = 8;
    q0_.push(x);
    q0_.front() = new_value;
    // x= new_value;
    EXPECT_EQ(q0_.length(), 1);
    EXPECT_EQ(q0_.front(), new_value);
}

// disable?
// slice:
TEST_F(queueTest, slice) {
    // Given:
    auto newQueue = my_queue<int>("tmp");
    newQueue.push(50);

    q0_.push(5);
    q0_.append(newQueue);

    // CHECK
    EXPECT_EQ(q0_.length(), 2);
}

TEST_F(queueTest, slice_empty) {
    // Given:
    GTEST_SKIP() << "Skipping single test";
    auto newQueue = my_queue<int>("tmp");
    newQueue.push(50);
#if 0
    // bug:
    // cannot be empty!
    q0_.push(1);
#endif
    // Do:   append:
    q0_.append(newQueue);
    // CHECK
    EXPECT_EQ(newQueue.length(), 0);
    EXPECT_EQ(q0_.length(), 1);
}

// swap
TEST_F(queueTest, push_backWorks) {
//given
    auto newQueue = my_queue<int>("tmp");
    newQueue.push(50);
    // do
    q0_.swap(newQueue);
    // check
    EXPECT_EQ(q0_.length(), 1);
    EXPECT_EQ(newQueue.length(), 0);
}

