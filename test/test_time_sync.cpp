#include "framework.h"
#include "../src/board/tdeck_plus/time_sync_policy.h"
#include "../src/board/tdeck_plus/timezone_policy.h"
#include "../src/ui/time_ui.h"

TEST(time_sync, gps_success_wins_and_source_is_visible) {
  TpTime::SyncPolicy p;
  CHECK(p.accept(TpTime::GPS,1767225600UL));
  CHECK_EQ(p.source,TpTime::GPS);
  CHECK(p.delivered);
}

TEST(time_sync, source_labels_report_the_trusted_source_not_sync_activity) {
  CHECK_EQ(std::string(TpTime::sourceLabel(TpTime::LOCAL)),std::string("TIME: LOCAL"));
  CHECK_EQ(std::string(TpTime::sourceLabel(TpTime::GPS)),std::string("TIME: GPS"));
  CHECK_EQ(std::string(TpTime::sourceLabel(TpTime::WIFI)),std::string("TIME: WIFI"));
  CHECK_EQ(std::string(TpTime::sourceLabel(TpTime::MANUAL)),std::string("TIME: MANUAL"));
}

TEST(time_sync, ntp_success_wins_when_gps_has_not_delivered) {
  TpTime::SyncPolicy p;
  CHECK(p.accept(TpTime::WIFI,1767225600UL));
  CHECK_EQ(p.source,TpTime::WIFI);
  CHECK(!p.accept(TpTime::GPS,1767225601UL));
}

TEST(time_sync, manual_fallback_does_not_fabricate_authority) {
  TpTime::SyncPolicy p; p.setManual();
  CHECK_EQ(p.source,TpTime::MANUAL); CHECK(!p.delivered);
  CHECK(p.accept(TpTime::GPS,1767225600UL));
}

TEST(time_sync, offline_catchup_can_only_be_consumed_once) {
  TpTime::SyncPolicy p; CHECK(p.accept(TpTime::GPS,1767225600UL));
  CHECK(p.consumeCatchup()); CHECK(!p.consumeCatchup());
}

TEST(time_sync, negative_and_absurd_source_deltas_are_rejected) {
  uint32_t saved=1767225600UL;
  CHECK(!TpTime::validDelta(saved-1,saved));
  CHECK(!TpTime::validDelta(saved+TpTime::MAX_DELTA+1,saved));
  CHECK(TpTime::validDelta(saved+60,saved));
  CHECK(!TpTime::validEpoch(1));
}

TEST(time_sync, wifi_disabled_or_unconfigured_blocks_fallback) {
  CHECK(!TpTime::SyncPolicy::wifiFallbackAllowed(false,true));
  CHECK(!TpTime::SyncPolicy::wifiFallbackAllowed(true,false));
  CHECK(TpTime::SyncPolicy::wifiFallbackAllowed(true,true));
}

TEST(time_sync, failed_sync_prompt_is_consumed_once_per_boot) {
  TpTime::SyncPolicy p;
  CHECK(p.consumeFailurePrompt(true));
  CHECK(!p.consumeFailurePrompt(true));
}

TEST(time_sync, open_settings_fields_refresh_from_new_wall_epoch) {
  int hour=1,minute=2;
  timeFieldsFromEpoch(1767269700UL,hour,minute); // 2026-01-01 12:15 UTC
  CHECK_EQ(hour,12); CHECK_EQ(minute,15);
}

TEST(time_sync, manual_catchup_requires_forward_sane_reference_and_confirmation) {
  TpTime::ManualCatchupPolicy p;
  uint32_t reference=1767225780UL,newTime=reference+7*3600+57*60;
  CHECK(p.prepare(reference,newTime)); CHECK_EQ(p.elapsed,7u*3600+57u*60);
  CHECK(!p.consume(reference,newTime+60));
  CHECK(p.consume(reference,newTime));
  CHECK(!p.consume(reference,newTime));
  CHECK(!p.prepare(reference,reference));
  CHECK(!p.prepare(reference,reference-1));
  CHECK(!p.prepare(reference,reference+TpTime::MAX_DELTA+1));
}

TEST(time_sync, utc_converts_to_eastern_standard_time) {
  uint32_t utc=TpTimezone::epoch(2026,1,15,17,0,0);
  CHECK_EQ(TpTimezone::offsetSeconds(TpTimezone::US_EASTERN,2026,utc),-5*3600);
  CHECK_EQ(utc+TpTimezone::offsetSeconds(TpTimezone::US_EASTERN,2026,utc),TpTimezone::epoch(2026,1,15,12,0,0));
}

TEST(time_sync, utc_converts_to_eastern_daylight_time) {
  uint32_t utc=TpTimezone::epoch(2026,7,15,16,0,0);
  CHECK_EQ(TpTimezone::offsetSeconds(TpTimezone::US_EASTERN,2026,utc),-4*3600);
  CHECK_EQ(utc+TpTimezone::offsetSeconds(TpTimezone::US_EASTERN,2026,utc),TpTimezone::epoch(2026,7,15,12,0,0));
}

TEST(time_sync, timezone_changes_presentation_not_authoritative_timeline) {
  uint32_t utc=TpTimezone::epoch(2026,7,15,16,0,0);
  uint32_t eastern=utc+TpTimezone::offsetSeconds(TpTimezone::US_EASTERN,2026,utc);
  uint32_t pacific=utc+TpTimezone::offsetSeconds(TpTimezone::US_PACIFIC,2026,utc);
  CHECK(eastern!=pacific); CHECK_EQ(utc,TpTimezone::epoch(2026,7,15,16,0,0));
}

TEST(time_sync, timezone_change_cannot_duplicate_authoritative_catchup) {
  TpTime::SyncPolicy p; CHECK(p.accept(TpTime::GPS,TpTimezone::epoch(2026,7,15,16)));
  CHECK(p.consumeCatchup());
  (void)TpTimezone::offsetSeconds(TpTimezone::US_EASTERN,2026,TpTimezone::epoch(2026,7,15,16));
  CHECK(!p.consumeCatchup());
}
