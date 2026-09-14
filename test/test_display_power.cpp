#include "framework.h"
#include "../src/board/tdeck_plus/display_power_policy.h"

TEST(display_power, timeout_choices_and_default_are_stable) {
  CHECK_EQ(TpDisplay::DEFAULT_TIMEOUT,(uint8_t)2);
  CHECK_EQ(TpDisplay::timeoutMs(0),30000UL);
  CHECK_EQ(TpDisplay::timeoutMs(1),60000UL);
  CHECK_EQ(TpDisplay::timeoutMs(2),120000UL);
  CHECK_EQ(TpDisplay::timeoutMs(3),300000UL);
  CHECK_EQ(TpDisplay::timeoutMs(4),600000UL);
  CHECK_EQ(TpDisplay::timeoutMs(5),0UL);
  CHECK_EQ(std::string(TpDisplay::timeoutLabel(5)),std::string("NEVER"));
  CHECK_EQ(TpDisplay::validIndex(99),TpDisplay::DEFAULT_TIMEOUT);
}

TEST(display_power, dim_precedes_timeout_and_never_stays_on) {
  CHECK(!TpDisplay::shouldDim(104999,2));
  CHECK(TpDisplay::shouldDim(105000,2));
  CHECK(!TpDisplay::shouldTurnOff(119999,2));
  CHECK(TpDisplay::shouldTurnOff(120000,2));
  CHECK(!TpDisplay::shouldDim(99999999,5));
  CHECK(!TpDisplay::shouldTurnOff(99999999,5));
}

TEST(display_power, newer_interaction_stamp_cannot_wrap_into_an_immediate_timeout) {
  CHECK_EQ(TpDisplay::idleMillis(1000,1002),0U);
  CHECK(!TpDisplay::shouldTurnOff(TpDisplay::idleMillis(1000,1002),0));
  CHECK_EQ(TpDisplay::idleMillis(5,0xFFFFFFF0UL),21U); // real millis rollover
}
