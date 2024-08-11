#include <gtest/gtest.h>

#include "../src/fork.h"
#include "../src/platform.h"
#include <gmock/gmock.h>
// fixme:
// #include "../src/machine.cpp"

class TestEvent : public PlatformEvent {
public:
    TestEvent(const Time time, const KeyCode keycode)
        : time(time),
          keycode(keycode) {
    }

private:
    Time time;
  KeyCode keycode;
  // press
};

// I want to mock this:
class testEnvironment final : public platformEnvironment {
  // virtual
  MOCK_METHOD(bool, press_p,(const PlatformEvent* event));
  MOCK_METHOD(bool, release_p,(const PlatformEvent* event));
  MOCK_METHOD(Time, time_of,(const PlatformEvent* event));
  MOCK_METHOD(KeyCode, detail_of,(const PlatformEvent* event));

  MOCK_METHOD(bool, ignore_event,(const PlatformEvent *pevent));

  // MOCK_METHOD(void, hand_over_event_to_next_plugin, (PlatformEvent* event));
  MOCK_METHOD(bool, output_frozen,());
  MOCK_METHOD(void, output_event,(PlatformEvent* pevent));
  MOCK_METHOD(void, push_time,(Time now));

  MOCK_METHOD(void, vlog,(const char* format, va_list argptr));
  void log(const char* format...) override
  {
    // ignore
  };
  MOCK_METHOD(void, log_event,(const std::string &message, const PlatformEvent *event));

  MOCK_METHOD(void, archive_event,(PlatformEvent* pevent, archived_event* archived_event));
  MOCK_METHOD(void, free_event,(PlatformEvent* pevent));
  MOCK_METHOD(void, rewrite_event,(PlatformEvent* pevent, KeyCode code));
};


using machineRec = forkingMachine<Time, KeyCode>;

class machineTest : public testing::Test {
protected:
    machineTest() : environment(new testEnvironment() ),
    fm (new machineRec(environment)) {

        // q0_ remains empty
        // q1_.push_back(1);
        // q2_.Enqueue(2);
        // q2_.Enqueue(3);
    }

    // ~QueueTest() override = default;
testEnvironment *environment;
    forkingMachine<Time, KeyCode> *fm;
    // Queue<int> q1_;
    // Queue<int> q2_;
};


//
TEST_F(machineTest, IsEmptyInitially) {
  auto pevent = new TestEvent(100L, 56);
  fm->accept_event(pevent);
  // expect call to
  // virtual void hand_over_event_to_next_plugin(PlatformEvent* event);
// CALLED

    //EXPECT_EQ(q0_.size(), 0);
}

