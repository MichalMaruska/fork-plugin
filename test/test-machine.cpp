#include <gtest/gtest.h>

#include "fork_enums.h"
#include <cstdlib>
#include <memory>

#if 0
#define ErrorF(fmt, ...)     printf(fmt, ##__VA_ARGS__)
#else
extern "C"
{
#include <cstdarg>
  void ErrorF(const char* format...)
  {
    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
  }
}
#endif

#include "../src/machine.h"
#include "../src/platform.h"
#include <gmock/gmock.h>

// I need archived_event
typedef struct
{
  Time time;
  KeyCode key;
  KeyCode forked;
  Bool press;                  /* client type? */
} archived_event;

// I need Environment which can convert into archived_event

// fixme:
// #include "../src/machine.cpp"

// using ::testing::Moc;
using testing::Mock;
using testing::Return;

// this is fully under control of our environment:
class TestEvent : public PlatformEvent {
public:
  archived_event *event;

  TestEvent(const Time time, const KeyCode keycode, bool press = true, const KeyCode forked = 0) {
    event = new ::archived_event({time, keycode, forked, press });
  }

};

// I want to mock this:
class testEnvironment final : public platformEnvironment<KeyCode, Time, archived_event> {
public:
  // virtual
  MOCK_METHOD(bool, press_p,(const PlatformEvent* event));
  MOCK_METHOD(bool, release_p,(const PlatformEvent* event));
  MOCK_METHOD(Time, time_of,(const PlatformEvent* event));
  MOCK_METHOD(KeyCode, detail_of,(const PlatformEvent* event));

  MOCK_METHOD(bool, ignore_event,(const PlatformEvent *pevent));

  // MOCK_METHOD(void, hand_over_event_to_next_plugin, (PlatformEvent* event));
  MOCK_METHOD(bool, output_frozen,());
  MOCK_METHOD(void, relay_event,(PlatformEvent* &pevent));
  MOCK_METHOD(void, push_time,(Time now));

  // MOCK_METHOD(void, vlog,(const char* format, va_list argptr));

  void vlog(const char* format, va_list argptr) override {
    vprintf(format, argptr);
  }

  void log(const char* format...) override
  {
    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
  };
  MOCK_METHOD(std::string, fmt_event,(const PlatformEvent *event));

  MOCK_METHOD(void, archive_event,(archived_event& ae, const PlatformEvent* event));
  MOCK_METHOD(void, free_event,(PlatformEvent* pevent));
  MOCK_METHOD(void, rewrite_event,(PlatformEvent* pevent, KeyCode code));

  MOCK_METHOD(std::unique_ptr<event_dumper<archived_event>>, get_event_dumper,());

};


using machineRec = forkingMachine<KeyCode, Time, archived_event>;
using fork_configuration = ForkConfiguration<KeyCode, Time, 256>;

class machineTest : public testing::Test {

protected:
    machineTest() : environment(new testEnvironment() ),
                    config (new fork_configuration),
                    fm (new machineRec(environment)) {
      // mmc: I could instead call forking_machine->create_configs();
      config->debug = 0;
      fm->config.reset(config);

      // q0_ remains empty
      // q1_.push_back(1);
      // q2_.Enqueue(2);
      // q2_.Enqueue(3);
    }

    // ~QueueTest() override = default;

  // fixme:
  ~machineTest()
  {
    // machine owns this:
    // config = nullptr;
    delete fm;
    // delete config;
    delete environment;
  }

  testEnvironment *environment;
  machineRec *fm;
  fork_configuration *config;
    // Queue<int> q1_;
    // Queue<int> q2_;
};



// When using a fixture, use TEST_F(TestFixtureClassName, TestName)
TEST_F(machineTest, AcceptEvent) {
  auto pevent = std::make_unique<TestEvent>( 100L, 56);

  // (pevent.get())
  EXPECT_CALL(*environment, relay_event);
// .WillOnce(ReturnRef(bar1)

  fm->lock();           // fixme: mouse must not interrupt us.
  fm->accept_event(std::move(pevent));
  // machine->next_decision_time()
  fm->unlock();
  // expect call to
  // virtual void hand_over_event_to_next_plugin(PlatformEvent* event);
  // CALLED
  // q0_.size()
  EXPECT_EQ(0, 0);

  // so for that EXPECT_CALL: this is necessary? as part of this test:
  Mock::VerifyAndClearExpectations(environment);
  // delete environment;
  // ::testing::Mock::AllowLeak(environment);
  // ::testing::Mock::VerifyAndClearExpectations(fm);
}

TEST_F(machineTest, Configure) {
  KeyCode A = 10;
  KeyCode B = 11;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  EXPECT_EQ(config->fork_keycode[A], B);
  // Mock::VerifyAndClearExpectations(environment);
  // verification_interval_of
}



// create event, non-forkable, pass, and expect it's got back, and freed?
TEST_F(machineTest, EventFreed) {

  // EXPECT_CALL(*environment, relay_event(pevent));
  // detail_of()

  // create env
  KeyCode A = 10;
  Time a_time = 156;
  Time next_time = 201;
  auto A_pevent = std::make_unique<TestEvent>(a_time, A);

  KeyCode B = 11;
  config->debug = 1;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  // EXPECT_EQ(config->fork_keycode[A], B);


  ON_CALL(*environment,ignore_event(A_pevent.get())).WillByDefault(Return(false));
  EXPECT_CALL(*environment,time_of(A_pevent.get())).WillRepeatedly(Return(a_time));
  // return:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));
  // many times:
  TestEvent* a = A_pevent.get();
  ON_CALL(*environment, detail_of(a)).WillByDefault(Return(A));
  ON_CALL(*environment, press_p(A_pevent.get())).WillByDefault(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
  EXPECT_CALL(*environment,push_time(a_time));
  EXPECT_CALL(*environment,push_time(a_time + next_time));

  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](PlatformEvent* pevent, KeyCode b) {
      auto event = static_cast<TestEvent*>(pevent)->event;
      event->key = b;
    });

  fm->lock();           // fixme: mouse must not interrupt us.
  fm->accept_event(std::move(A_pevent));
  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // (a)
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(a)); // (nullptr)

  fm->step_in_time_locked(a_time + next_time);
  fm->unlock();

  Mock::VerifyAndClearExpectations(environment);
}


TEST_F(machineTest, ForkBySecond) {
  // configure
  KeyCode A = 10;
  Time a_time = 156;
  auto A_pevent = std::make_unique<TestEvent>(a_time, A);
  KeyCode F = 11;

  Time b_time = a_time + 50;
  KeyCode B = 60;
  auto B_pevent = std::make_unique<TestEvent>(b_time, B);

  Time b_release_time = b_time + 50;
  auto B_release_pevent = std::make_unique<TestEvent>(b_release_time, B, false);

  config->debug = 1;
  fm->configure_key(fork_configure_key_fork, A, F, 1); // 1 means SET
  // EXPECT_EQ(config->fork_keycode[A], F);

  TestEvent* a = A_pevent.get();
  TestEvent* b = B_pevent.get();
  EXPECT_CALL(*environment,ignore_event(A_pevent.get())).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment,time_of(A_pevent.get())).WillRepeatedly(Return(a_time));


  EXPECT_CALL(*environment,time_of(B_pevent.get())).WillRepeatedly(Return(b_time));
  EXPECT_CALL(*environment,ignore_event(B_pevent.get())).WillRepeatedly(Return(false));

  // return:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));
  // many times:
  EXPECT_CALL(*environment, detail_of(a)).WillRepeatedly(Return(A));
  EXPECT_CALL(*environment, press_p(A_pevent.get())).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, detail_of(b)).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, press_p(B_pevent.get())).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, press_p(B_release_pevent.get())).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, release_p(B_release_pevent.get())).WillRepeatedly(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
  EXPECT_CALL(*environment,push_time(a_time));
  EXPECT_CALL(*environment,push_time(b_time));

  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](PlatformEvent* pevent, KeyCode b) {
      auto event = static_cast<TestEvent*>(pevent)->event;
      event->key = b;
    });

  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // todo: check it's F
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(a)); // (nullptr)


  fm->lock();           // fixme: mouse must not interrupt us.
  fm->accept_event(std::move(A_pevent));
  fm->accept_event(std::move(B_pevent));
  fm->accept_event(std::move(B_release_pevent));


  fm->unlock();

  Mock::VerifyAndClearExpectations(environment);
}



// Thus your main() function must return the value of RUN_ALL_TESTS().
// Calling it more than once conflicts with some advanced GoogleTest features (e.g., thread-safe death tests)
// RUN_ALL_TESTS()


// gtest_main (as opposed to with gtest

