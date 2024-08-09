#include <gtest/gtest.h>

#include "../src/queue.h"

class queueTest : public testing::Test {
protected:
    queueTest():
        q0_("test")
    {
    };
    my_queue<int> q0_;
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
    EXPECT_EQ(*q0_.front(), old_value);
}

TEST_F(queueTest, oneElementPointer) {
// push by pointer:
    int x = 7;
    int new_value = 8;
    q0_.push(&x);
    x= new_value;
    EXPECT_EQ(q0_.length(), 1);
    EXPECT_EQ(*q0_.front(), new_value);
}

// disable?
// slice:
TEST_F(queueTest, slice) {
    // Given:
    auto newQueue = my_queue<int>("tmp");
    newQueue.push(50);

    // Do:   append:
    q0_.slice(newQueue);
    // CHECK
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