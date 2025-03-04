#include <gtest/gtest.h>

#include "fork_enums.h"
#include <cstdlib>
#include <memory>
#include <ostream>

#include "../src/machine.h"
#include "../src/platform.h"
#include "empty_last.h"

#include <gmock/gmock.h>

typedef int Time;
typedef int KeyCode;


// I need archived_event
typedef struct
{
  Time time;
  KeyCode key;
  KeyCode forked;
  bool press;                  /* client type? */
} archived_event;

using testing::Mock;
using testing::Return;
using testing::AnyNumber;

// I need Environment which can convert into archived_event
// This is fully under control of our environment:
class TestEvent {
public:
  archived_event *event;

  TestEvent(const Time time, const KeyCode keycode, bool press = true, const KeyCode forked = 0) {
    event = new ::archived_event({time, keycode, forked, press });
  }

};

// I want to mock this:
class testEnvironment final : public forkNS::platformEnvironment<KeyCode, Time,
                                                                 archived_event, TestEvent>{
public:
  // virtual
  MOCK_METHOD(bool, press_p,(const TestEvent& event),(const));
  MOCK_METHOD(bool, release_p,(const TestEvent& event), (const));
  MOCK_METHOD(Time, time_of,(const TestEvent& event), (const));
  MOCK_METHOD(KeyCode, detail_of,(const TestEvent& event), (const));

  MOCK_METHOD(bool, ignore_event,(const TestEvent &pevent));

  MOCK_METHOD(bool, output_frozen,());
  MOCK_METHOD(void, relay_event,(const TestEvent &pevent), (const));
  MOCK_METHOD(void, push_time,(Time now));

  // MOCK_METHOD(void, vlog,(const char* format, va_list argptr));

  void vlog(const char* format, va_list argptr) const override {
    vprintf(format, argptr);
  }

  void log(const char* format...) const override
  {
    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
  };
  MOCK_METHOD(void, fmt_event,(const char* message, const TestEvent &event), (const));

  MOCK_METHOD(void, archive_event,(archived_event& ae, const TestEvent& event));
  MOCK_METHOD(void, free_event,(TestEvent* pevent), (const));
  MOCK_METHOD(void, rewrite_event,(TestEvent& pevent, KeyCode code));

  MOCK_METHOD(std::unique_ptr<forkNS::event_dumper<archived_event>>, get_event_dumper,());
};


using last_events_t = empty_last_events_t<archived_event>;
using machineRec = forkNS::forkingMachine<KeyCode, Time,
                                          TestEvent, testEnvironment,
                                          archived_event, last_events_t>;
using fork_configuration = machineRec::fork_configuration;

// template instantiation
namespace forkNS {
  // explicit template instantiation
  template class forkingMachine<KeyCode, Time, TestEvent, testEnvironment, archived_event, last_events_t>;
}


class machineTest : public testing::Test {

protected:
    machineTest() : environment(new testEnvironment() ),
                    config (new fork_configuration),
                    fm (new machineRec(environment)) {

      // mmc: I could instead call forking_machine->create_configs();
      config->debug = 0;
      fm->config.reset(config);
    }

  ~machineTest()
  {
    // machine `owns' this:
    // config = nullptr;
    // so don't do this:
    // delete config;

    delete fm;
    delete environment;
  }

  testEnvironment *environment;
  machineRec *fm;
  fork_configuration *config;
};



// When using a fixture, use TEST_F(TestFixtureClassName, TestName)
TEST_F(machineTest, AcceptEvent) {
  TestEvent pevent(100L, 56);

  EXPECT_CALL(*environment, relay_event);

  Time next = fm->accept_event(pevent);

  // expect calls:
  // so for that EXPECT_CALL: this is necessary? as part of this test:
  Mock::VerifyAndClearExpectations(environment);
  // ::testing::Mock::AllowLeak(environment);
}

TEST_F(machineTest, Configure) {
  KeyCode A = 10;
  KeyCode B = 11;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  EXPECT_EQ(config->fork_keycode[A], B);

  Mock::VerifyAndClearExpectations(environment);
}

#if 0
// fixme: I need equal_to()


// create event, non-forkable, pass, and expect it's got back, and freed?
TEST_F(machineTest, EventFreed) {

  KeyCode A = 10;
  Time a_time = 156;
  Time next_time = 201;
  TestEvent A_pevent(a_time, A);

  KeyCode B = 11;
  config->debug = 1;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  // EXPECT_EQ(config->fork_keycode[A], B);

  // Emulate next plugin:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));


  ON_CALL(*environment, ignore_event(A_pevent)).WillByDefault(Return(false));
  EXPECT_CALL(*environment, time_of(A_pevent)).WillRepeatedly(Return(a_time));

  // many times:
  TestEvent& a = A_pevent;
  EXPECT_CALL(*environment, detail_of(a)).Times(AnyNumber()).WillRepeatedly(Return(A));
  ON_CALL(*environment, press_p(A_pevent)).WillByDefault(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
  //  EXPECT_CALL(*environment, push_time(a_time));
  EXPECT_CALL(*environment, push_time(a_time + next_time));

  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](TestEvent& pevent, KeyCode b) {
      auto event = static_cast<TestEvent&>(pevent).event;
      event->key = b;
    });

  fm->accept_event(A_pevent);
#if 1
  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // (a)
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(&a)); // (nullptr)
#endif

  fm->accept_time(a_time + next_time);

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, ForkBySecond) {
  // configure
  KeyCode A = 10;
  Time a_time = 156;
  TestEvent A_pevent(a_time, A);
  KeyCode F = 11;

  Time b_time = a_time + 50;
  KeyCode B = 60;
  TestEvent B_pevent(b_time, B);

  Time b_release_time = b_time + 50;
  TestEvent B_release_pevent (b_release_time, B, false);

  config->debug = 1;
  fm->configure_key(fork_configure_key_fork, A, F, 1); // 1 means SET
  // EXPECT_EQ(config->fork_keycode[A], F);

  TestEvent& a = A_pevent;
  TestEvent& b = B_pevent;

  std::cout << "A: " << &a << " b:" << &b << " br:" << &B_release_pevent << std::endl;

  // return:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));

  // many times:
  EXPECT_CALL(*environment, ignore_event(A_pevent)).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, time_of(A_pevent)).WillRepeatedly(Return(a_time));
  EXPECT_CALL(*environment, detail_of(a)).WillRepeatedly(Return(A));
  EXPECT_CALL(*environment, press_p(A_pevent)).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, time_of(B_pevent)).WillRepeatedly(Return(b_time));
  EXPECT_CALL(*environment, ignore_event(B_pevent)).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, detail_of(b)).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, press_p(B_pevent)).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, detail_of(B_release_pevent)).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, time_of(B_release_pevent)).WillRepeatedly(Return(b_release_time));
  EXPECT_CALL(*environment, press_p(B_release_pevent)).WillRepeatedly(Return(false));
  // EXPECT_CALL(*environment, release_p(B_release_pevent)).WillRepeatedly(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
#if 0
  EXPECT_CALL(*environment,push_time(a_time));
  EXPECT_CALL(*environment,push_time(b_time));
  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](TestEvent* pevent, KeyCode b) {
      auto event = static_cast<TestEvent*>(pevent)->event;
      event->key = b;
    });
#endif

#if 0
  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // todo: check it's F
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(a)); // (nullptr)
#endif

  fm->accept_event(std::move(A_pevent));
  fm->accept_event(std::move(B_pevent));
  fm->accept_event(std::move(B_release_pevent));

  Mock::VerifyAndClearExpectations(environment);
}

#endif


// Thus your main() function must return the value of RUN_ALL_TESTS().
// Calling it more than once conflicts with some advanced GoogleTest features (e.g., thread-safe death tests)
// RUN_ALL_TESTS()

// gtest_main (as opposed to with gtest
