#pragma once
#if TAMAPOKE_TDECK_PLUS
// Included after sketch helpers: alternate presentation/action routing only.
// Pet remains the single owner of rules, rewards, progression, and saves.
static void compactText(const char *text, int x, int y, int width, uint8_t size=1) {
  setSize(size);
  char buf[128]; snprintf(buf,sizeof(buf),"%s",text);
  size_t n = strlen(buf);
  while (n && textW(buf,size) > width) {
    // Remove a whole UTF-8 code point, never leave a split character.
    do { --n; } while (n && (uint8_t(buf[n]) & 0xC0) == 0x80);
    buf[n] = 0;
  }
  setCur(x,y); printT(buf);
}
static void compactFill(const Layout::Rect &r, uint16_t color) {
  gfx->fillRect(r.x,r.y,r.w,r.h,color);
}
static void compactButton(const Layout::Rect &r, const char *label, uint16_t color=UI_WHITE) {
  gfx->fillRoundRect(r.x,r.y,r.w,r.h,6,color);
  gfx->drawRoundRect(r.x,r.y,r.w,r.h,6,UI_INK);
  gfx->setTextColor(UI_INK);
  uint8_t size = textW(label,2) <= r.w-12 ? 2 : 1;
  compactText(label,r.x+6,r.y+(r.h-8*size)/2,r.w-12,size);
}
#include "actions_landscape.h"
void renderCompact() {
  if (pet.awaitingStarter()) {
    gfx->fillScreen(UI_BG_DAY); gfx->setTextColor(UI_INK);
    compactText(T(S_CHOOSE_STARTER),12,12,296,2);
    for (int i=0;i<3;++i) {
      const auto &r=Layout::starter[i];
      compactButton(r, "", lerp565(DEX_TBL[STARTER_DEX[i]].accent,UI_WHITE,6,8));
      const uint8_t *th=thumbs.get(STARTER_DEX[i]);
      if (th) drawThumb(th,r.x+4,r.y+4,1,false);
      else drawMap(SPECIES[flashIdxForDex(STARTER_DEX[i])].sprite,32,r.x+6,r.y+12,1,false);
      compactText(dexName(STARTER_DEX[i]),r.x+70,r.y+20,r.w-78,2);
    }
    gfx->flush(); return;
  }
  int hour=sceneHour(); gNight=pet.sleeping || hour<6 || hour>=20;
  gfx->fillScreen(gNight ? UI_BG_NIGHT : UI_BG_DAY);
  compactFill(Layout::scene,gNight ? C565(0x20,0x30,0x48) : C565(0xBB,0xDF,0xE2));
  gfx->drawFastHLine(Layout::scene.x,PET_GROUND,Layout::scene.w,gNight ? UI_TRACK : UI_BAR_OK);
  if (pet.isEgg()) {
    int zoom=3, x=CX-48,y=PET_CY-48;
    drawMap(SPR_EGG,32,x,y,zoom,false);
    if (pet.eggCracks()>=1) for (auto &c:CRACK1) gfx->fillRect(x+c[0]*zoom,y+c[1]*zoom,zoom,zoom,INK_K);
    if (pet.eggCracks()>=2) for (auto &c:CRACK2) gfx->fillRect(x+c[0]*zoom,y+c[1]*zoom,zoom,zoom,INK_K);
  } else {
    drawPet();
    drawBath(); // Includes completion: do not skip while a bath is pending.
    for (int i=0;i<pet.poops && i<5;++i) drawMap(SPR_POOP,32,4+i*34,164,1,false);
    if (pet.showHeart()) drawMap(SPR_HEART,32,170,28,1,false);
  }
  // Repaint reserved UI after scene effects, keeping all controls legible.
  compactFill(Layout::header,gNight ? UI_BG_NIGHT : UI_BG_DAY);
  compactFill(Layout::sidebar,gNight ? UI_BG_NIGHT : UI_BG_DAY);
  compactFill(Layout::toolbar,gNight ? UI_BG_NIGHT : UI_BG_DAY);
  gfx->setTextColor(inkColor());
  char title[80];
  if (pet.isEgg()) snprintf(title,sizeof(title),"%s",T(S_EGG_HDR));
  else snprintf(title,sizeof(title),T(S_NAME_FMT),pet.shiny ? "*" : "",pet.nick[0] ? pet.nick : dexName(pet.speciesId),pet.level());
  compactText(title,6,5,308,2);
  if (pet.isEgg()) {
    compactText(eggMsg(),Layout::eggMessage.x,Layout::eggMessage.y,Layout::eggMessage.w,1);
    compactText(pet.eggRarity() == R_LEGENDARIO ? T(S_EGG_LEGEND) : pet.eggRarity() >= R_RARO ? T(S_EGG_RARE) : "",Layout::eggDetails.x,Layout::eggDetails.y,Layout::eggDetails.w);
  } else {
    const char *labels[4]={T(S_BAR_FOOD),T(S_BAR_JOY),T(S_BAR_ENE),T(S_BAR_HYG)};
    uint8_t values[4]={pet.fullness,pet.joy,pet.energy,pet.hygiene};
    for (int i=0;i<4;++i) {
      const auto &r=Layout::needs[i];
      int y=r.y;
      compactText(labels[i],r.x,y,r.w-31);
      char number[5]; snprintf(number,sizeof(number),"%u",values[i]);
      compactText(number,r.x+r.w-23,y,27);
      gfx->fillRect(r.x,y+12,r.w,6,UI_TRACK);
      gfx->fillRect(r.x,y+12,r.w*values[i]/100,6,values[i]<30 ? UI_BAR_BAD : UI_BAR_OK);
    }
    compactText(statusMsg(),Layout::status.x,Layout::status.y,Layout::status.w);
    for (int i=0;i<4;++i) {
      const auto &r=Layout::care[i]; compactButton(r,"",pet.sleeping && i!=2 ? UI_TRACK : UI_WHITE);
      drawMap(buttons[i].icon,16,r.x+24,r.y+4,2,false);
    }
    if (pet.wantEvolveButton()) compactButton(Layout::progression,T(S_EVO_TAP),UI_BAR_WARN);
    else if (pet.canRunawayNow()) compactButton(Layout::progression,T(S_RUNAWAY),UI_BAR_BAD);
    else if (pet.wantFarewellButton()) compactButton(Layout::progression,T(S_FAR_GO),UI_BAR_WARN);
  }
  if (pet.ceremony) {
    const char *msg=pet.ceremony==CER_FAREWELL ? T(S_FAREWELL) : pet.ceremony==CER_RUNAWAY ? T(S_RUNAWAY) : T(S_GOODBYE);
    compactFill(Layout::toolbar,UI_BG_DAY); gfx->setTextColor(UI_INK);
    compactText(msg,8,212,304,1);
  }
  if (feedMenuUntil && !timeLeft(feedMenuUntil)) feedMenuUntil=0;
  if (confirmUntil && !timeLeft(confirmUntil)) confirmUntil=0;
  if (choiceKind && !timeLeft(choiceUntil)) choiceKind=0;
  if (feedMenuUntil) {
    const char *const *icons[4]={SPR_ICON_FOOD,SPR_ICON_BERRY_B,SPR_ICON_BERRY_G,SPR_ICON_CANDY};
    for (int i=0;i<4;++i) { const auto &r=Layout::food[i]; compactButton(r,""); drawMap(icons[i],16,r.x+20,r.y+8,2,false); }
  }
  if (choiceKind || confirmUntil) {
    const auto &r=Layout::prompt;
    gfx->fillRoundRect(r.x,r.y,r.w,r.h,8,UI_WHITE); gfx->setTextColor(UI_INK);
    char question[96];
    if (confirmUntil) snprintf(question,sizeof(question),T(S_RELEASE_FMT),dexName(pet.speciesId));
    else snprintf(question,sizeof(question),"%s",choiceKind==1 ? T(S_EVO_Q) : T(S_FAR_Q));
    compactText(question,24,66,272,1);
    compactButton(Layout::accept,confirmUntil ? T(S_YES) : choiceKind==1 ? T(S_EVO_TAP) : T(S_FAR_GO),UI_BAR_WARN);
    compactButton(Layout::cancel,confirmUntil ? T(S_NO) : choiceKind==1 ? T(S_EVO_KEEP) : T(S_FAR_STAY));
  }
  gfx->flush();
}
#endif
