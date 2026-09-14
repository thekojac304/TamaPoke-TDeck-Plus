#include "framework.h"
#include "../src/board/tdeck_plus/battery_policy.h"

TEST(battery, rejects_invalid_adc_conversions_and_clamps_curve) {
  CHECK(!TpBattery::validMillivolts(2999)); CHECK(TpBattery::validMillivolts(3700));
  CHECK(!TpBattery::validMillivolts(4401));
  CHECK_EQ(TpBattery::batteryMillivoltsFromAdc(1900),3800);
  CHECK_EQ(TpBattery::estimatePercent(3000),0);
  CHECK_EQ(TpBattery::estimatePercent(4190),100);
  CHECK_EQ(TpBattery::estimatePercent(4500),100);
}

TEST(battery, interpolation_smoothing_and_hysteresis_are_deterministic) {
  CHECK_EQ(TpBattery::estimatePercent(3720),50);
  CHECK_EQ(TpBattery::estimatePercent(3845),65);
  CHECK_EQ(TpBattery::smoothMillivolts(3800,3880),3810);
  CHECK_EQ(TpBattery::stabilizePercent(50,51),50);
  CHECK_EQ(TpBattery::stabilizePercent(50,52),52);
}
