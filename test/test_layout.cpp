#include "framework.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#define TAMAPOKE_TDECK_PLUS 1
#include "../src/ui/layout.h"

TEST(layout, controls_are_visible_and_do_not_overlap_scene) {
  auto visible=[](const Layout::Rect &r) {
    CHECK(r.x>=0 && r.y>=0 && r.w>=40 && r.h>=40);
    CHECK(r.x+r.w<=320 && r.y+r.h<=240);
    CHECK(r.contains(r.x,r.y));
    CHECK(r.contains(r.x+r.w-1,r.y+r.h-1));
    CHECK(!r.contains(r.x+r.w,r.y));
    CHECK(!r.contains(r.x,r.y+r.h));
  };
  for (auto &r:Layout::starter) visible(r);
  for (auto &r:Layout::care) { visible(r); CHECK(r.y>=Layout::scene.y+Layout::scene.h); }
  for (auto &r:Layout::food) visible(r);
  visible(Layout::accept); visible(Layout::cancel); visible(Layout::progression);
  for (int y=200;y<240;++y) for (int x=0;x<320;++x) {
    int hits=0;
    for (auto &r:Layout::care) if (r.contains(x,y)) ++hits;
    CHECK_EQ(hits,1);
  }
}

TEST(layout, all_repository_pmd_action_frames_fit_without_resampling) {
  namespace fs=std::filesystem;
  fs::path dir="../tools/sdcard/mons";
  if (!fs::exists(dir)) dir="tools/sdcard/mons";
  CHECK(fs::exists(dir));
  if (!fs::exists(dir)) return;
  int packs=0, actions=0;
  for (auto &entry:fs::directory_iterator(dir)) {
    std::ifstream file(entry.path(),std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    if (bytes.size()<7 || memcmp(bytes.data(),"TPK2",4)) continue;
    ++packs;
    size_t pos=7+2*(bytes[5]+256*bytes[6]);
    for (int i=0;i<bytes[4];++i) {
      CHECK(pos+4<=bytes.size()); if (pos+4>bytes.size()) break;
      int w=bytes[pos+1],h=bytes[pos+2],frames=bytes[pos+3];
      int scale=Layout::fitScale(w,h,5);
      CHECK(scale>=1 && scale<=5);
      CHECK_MSG(w*scale<=Layout::scene.w,entry.path().string());
      CHECK_MSG(h*scale<=Layout::scene.h,entry.path().string());
      pos+=4+frames*2+w*h*frames;
      CHECK(pos<=bytes.size());
      ++actions;
    }
  }
  CHECK_EQ(packs,302);
  CHECK(actions>302);
  CHECK_EQ(Layout::fitScale(72,128,5),1); // Gyarados
  CHECK_EQ(Layout::fitScale(32,32,5),5);
}

TEST(layout, secondary_controls_and_generated_cells_fit_landscape) {
  auto fits=[](const Layout::Rect &r) {
    CHECK(r.x>=0); CHECK(r.y>=0); CHECK(r.w>0); CHECK(r.h>0);
    CHECK(r.x+r.w<=320); CHECK(r.y+r.h<=240);
  };
  fits(Layout::homeProfile); fits(Layout::homeDex); fits(Layout::homeSettings);
  fits(Layout::screenBack); fits(Layout::cardHome); fits(Layout::nicknameField);
  fits(Layout::trainButton); fits(Layout::hourMinus); fits(Layout::hourPlus);
  fits(Layout::minuteMinus); fits(Layout::minutePlus); fits(Layout::soundToggle);
  fits(Layout::languageToggle); fits(Layout::clockApply);
  fits(Layout::galleryPrev); fits(Layout::galleryNext); fits(Layout::galleryExit);
  fits(Layout::detailSprite); fits(Layout::detailInfo); fits(Layout::detailBack);
  fits(Layout::sackExit); fits(Layout::sackTarget); fits(Layout::gameViewport);
  fits(Layout::gameHud); fits(Layout::gameExit);
  for (int i=0;i<4;++i) fits(Layout::cardTabs[i]);
  for (int i=0;i<30;++i) {
    auto key=Layout::keyboardKey(i); fits(key);
    CHECK(Layout::keyboardArea.contains(key.x,key.y));
    CHECK(Layout::keyboardArea.contains(key.x+key.w-1,key.y+key.h-1));
  }
  for (int i=0;i<16;++i) {
    auto cell=Layout::galleryCell(i); fits(cell);
    CHECK(Layout::galleryGrid.contains(cell.x,cell.y));
    CHECK(Layout::galleryGrid.contains(cell.x+cell.w-1,cell.y+cell.h-1));
  }
}
