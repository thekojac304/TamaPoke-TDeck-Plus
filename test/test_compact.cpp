#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#define TAMAPOKE_TDECK_PLUS 1
#include "../src/ui/layout.h"
namespace {
Pet pet;
constexpr int16_t STARTER_DEX[3]={1,4,7};
uint32_t choiceUntil=0,confirmUntil=0,feedMenuUntil=0;
uint8_t choiceKind=0;
int baths=0;
bool galleryOpen=false,kbOpen=false,clockOpen=false,cardOpen=false,gameOpen=false,sackOpen=false;
bool galleryDirty=false;
int galleryPage=0;
int16_t galleryDetail=0;
uint8_t cardPage=0;
int routedSecondary=0;
struct { void load(int16_t,bool) {} } evoPmd;
bool timeLeft(uint32_t t) { return t && (int32_t)(t-millis())>0; }
bool inPetZone(int16_t x,int16_t y) { return Layout::scene.contains(x,y); }
void startBath() { if (!pet.sleeping && !pet.isEgg()) ++baths; }
void startGame() { if (!pet.sleeping&&!pet.isEgg()) gameOpen=true; }
void openClock() { clockOpen=true; }
void compactGalleryTap(int16_t,int16_t) { ++routedSecondary; }
void compactKeyboardTap(int16_t,int16_t) { ++routedSecondary; }
void compactClockTap(int16_t,int16_t) { ++routedSecondary; }
void compactCardTap(int16_t,int16_t) { ++routedSecondary; }
void compactGameTap(int16_t,int16_t) { ++routedSecondary; }
#include "../src/ui/actions_landscape.h"
void fresh(bool hatch=true) {
  mockNvsReset(); mockSetMillis(100);
  pet=Pet(); pet.begin(); choiceKind=0; choiceUntil=confirmUntil=feedMenuUntil=0; baths=0;
  galleryOpen=kbOpen=clockOpen=cardOpen=gameOpen=sackOpen=false;
  galleryPage=galleryDetail=cardPage=0; galleryDirty=false; routedSecondary=0;
  if (hatch) { pet.chooseStarter(1); pet.eggTap(); pet.eggTap(); pet.eggTap(); }
}
void tap(const Layout::Rect &r) { compactTap(r.x+r.w/2,r.y+r.h/2); }
}
TEST(compact, each_starter_and_egg_are_touch_accessible) {
  for (int i=0;i<3;++i) {
    fresh(false);
    compactTap(0,0); CHECK(pet.awaitingStarter());
    tap(Layout::starter[i]); CHECK(!pet.awaitingStarter());
    CHECK(pet.isEgg());
    compactTap(100,100); compactTap(100,100); compactTap(100,100);
    CHECK(!pet.isEgg()); CHECK_EQ(pet.speciesId,STARTER_DEX[i]);
  }
}
TEST(compact, play_light_bath_and_affection_route_to_existing_care) {
  fresh(); pet.joy=30; pet.energy=60; pet.fullness=60;
  tap(Layout::care[1]);
  CHECK(gameOpen);
  gameOpen=false;
  tap(Layout::care[3]); CHECK_EQ(baths,1);
  compactTap(100,100); CHECK_EQ(pet.joy,35);
  tap(Layout::care[2]); CHECK(pet.sleeping);
  tap(Layout::care[0]); CHECK_EQ(feedMenuUntil,0U);
  tap(Layout::care[1]); CHECK_EQ(pet.joy,35);
  CHECK(!gameOpen);
  tap(Layout::care[3]); CHECK_EQ(baths,1);
  tap(Layout::care[2]); CHECK(!pet.sleeping);
}

TEST(compact, visible_home_navigation_opens_secondary_screens) {
  fresh();
  tap(Layout::homeProfile); CHECK(cardOpen); CHECK_EQ(cardPage,0);
  cardOpen=false; tap(Layout::homeDex); CHECK(galleryOpen); CHECK(galleryDirty);
  galleryOpen=false; tap(Layout::homeSettings); CHECK(clockOpen);
}
TEST(compact, all_food_choices_match_engine_results_and_dismiss_popup) {
  for (int i=0;i<4;++i) {
    fresh(); pet.fullness=40;
    Pet expected=pet;
    if (i==3) expected.feedCandy(); else expected.feedBerry(i);
    tap(Layout::care[0]); CHECK(feedMenuUntil!=0);
    tap(Layout::food[i]); CHECK_EQ(feedMenuUntil,0U);
    CHECK_EQ(pet.fullness,expected.fullness); CHECK_EQ(pet.joy,expected.joy);
    CHECK_EQ(pet.weight,expected.weight); CHECK_EQ(pet.bond,expected.bond);
  }
}
TEST(compact, release_requires_live_confirmation_and_accept_rectangle) {
  fresh(); confirmUntil=millis()+1000;
  tap(Layout::cancel); CHECK_EQ(pet.ceremony,CER_NONE); CHECK_EQ(confirmUntil,0U);
  confirmUntil=millis()+1000; mockAdvanceMillis(1001);
  tap(Layout::accept); CHECK_EQ(pet.ceremony,CER_NONE);
  confirmUntil=millis()+1000; tap(Layout::accept); CHECK_EQ(pet.ceremony,CER_RELEASE);
  mockAdvanceMillis(CEREMONY_MS+1); pet.update(millis()); CHECK(pet.isEgg());
}
