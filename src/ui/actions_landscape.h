#pragma once
// Compact touch routing; uses the existing Pet API without new game rules.
void compactTap(int16_t x, int16_t y) {
  if (pet.awaitingStarter()) {
    for (int i=0;i<3;++i) if (Layout::starter[i].contains(x,y)) pet.chooseStarter(STARTER_DEX[i]);
    return;
  }
  if (galleryOpen) { compactGalleryTap(x,y); return; }
  if (kbOpen) { compactKeyboardTap(x,y); return; }
  if (clockOpen) { compactClockTap(x,y); return; }
  if (cardOpen) { compactCardTap(x,y); return; }
  if (gameOpen) { compactGameTap(x,y); return; }
  if (sackOpen) return;  // taps are counted on touch-down in handleTouch()
  if (pet.ceremony) return;
  if (choiceKind) {
    bool yes = Layout::accept.contains(x,y), no = Layout::cancel.contains(x,y);
    if (!yes && !no) return;
    if (timeLeft(choiceUntil)) {
      if (choiceKind == 1) {
        if (yes) { int16_t old=pet.speciesId; pet.evolve(); evoPmd.load(old,pet.shiny); }
        else pet.declineEvolve();
      } else {
        if (yes) pet.startFarewell(); else pet.declineFarewell();
      }
    }
    choiceKind=0; return;
  }
  if (confirmUntil) {
    if (timeLeft(confirmUntil) && Layout::accept.contains(x,y)) pet.release();
    confirmUntil=0; return;
  }
  if (feedMenuUntil) {
    if (timeLeft(feedMenuUntil)) for (int i=0;i<4;++i) if (Layout::food[i].contains(x,y)) {
      if (i==3) pet.feedCandy(); else pet.feedBerry(i);
    }
    feedMenuUntil=0; return;
  }
  if (pet.isEgg()) { pet.eggTap(); return; }
  if (Layout::homeProfile.contains(x,y)) { cardOpen=true; cardPage=0; return; }
  if (Layout::homeDex.contains(x,y)) {
    galleryOpen=true; galleryPage=0; galleryDetail=0; galleryDirty=true; return;
  }
  if (Layout::homeSettings.contains(x,y)) { openClock(); return; }
  if (Layout::progression.contains(x,y)) {
    if (pet.wantEvolveButton()) { choiceKind=1; choiceUntil=millis()+12000; return; }
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind=2; choiceUntil=millis()+12000; return; }
  }
  for (int i=0;i<4;++i) if (Layout::care[i].contains(x,y)) {
    if (i==0 && !pet.sleeping) feedMenuUntil=millis()+6000;
    else if (i==1) startGame();
    else if (i==2) pet.toggleLight();
    else if (i==3) startBath();
    return;
  }
  if (inPetZone(x,y)) pet.caress();
}
