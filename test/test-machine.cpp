#include <gtest/gtest.h>

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


// fixme:
// #include "../src/machine.cpp"

// using ::testing::Moc;
using testing::Mock;

class TestEvent : public PlatformEvent {
public:
    TestEvent(const Time time, const KeyCode keycode, bool press = true, const KeyCode forked = 0) {
      event = new ::archived_event({time, keycode, forked, press });
    }

private:
  archived_event *event;

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
  MOCK_METHOD(void, relay_event,(PlatformEvent* pevent));
  MOCK_METHOD(void, push_time,(Time now));

  MOCK_METHOD(void, vlog,(const char* format, va_list argptr));
  void log(const char* format...) override
  {
    // ignore
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
      // I could instead call forking_machine->create_configs();
        // q0_ remains empty
        // q1_.push_back(1);
        // q2_.Enqueue(2);
        // q2_.Enqueue(3);
      config->debug = 0;
      fm->config.reset(config);
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
  auto pevent = new TestEvent(100L, 56);

  EXPECT_CALL(*environment, relay_event(pevent));
// .WillOnce(ReturnRef(bar1)

  fm->lock();           // fixme: mouse must not interrupt us.
  fm->accept_event(pevent);
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


// Thus your main() function must return the value of RUN_ALL_TESTS().
// Calling it more than once conflicts with some advanced GoogleTest features (e.g., thread-safe death tests)
// RUN_ALL_TESTS()


// gtest_main (as opposed to with gtest

