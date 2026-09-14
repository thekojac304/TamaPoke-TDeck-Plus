#pragma once
#if TAMAPOKE_TDECK_PLUS

static void compactDrawThumb(const uint8_t *b,const Layout::Rect &cell,int scale,bool silhouette);

static void compactCentered(const char *text, const Layout::Rect &r, uint8_t size=1) {
  setSize(size);
  int x=r.x+(r.w-textW(text,size))/2;
  compactText(text,x<r.x?r.x:x,r.y+(r.h-8*size)/2,r.w,size);
}

static void compactScreenTitle(const char *title) {
  gfx->fillScreen(UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  compactText(title,8,8,256,2);
  compactButton(Layout::screenBack,"X",UI_WHITE);
}

static void compactCardChrome() {
  static const char *const tab[4]={"P","B","M","+"};
  for (int i=0;i<4;++i)
    compactButton(Layout::cardTabs[i],tab[i],i==cardPage?UI_BAR_WARN:UI_WHITE);
  compactButton(Layout::cardHome,T(S_BACK),UI_WHITE);
}

static void compactStatRow(int y,const char *label,uint16_t value,uint16_t maxValue,uint16_t color) {
  gfx->setTextColor(UI_INK);
  compactText(label,12,y,76,1);
  char number[8]; snprintf(number,sizeof(number),"%u",value);
  compactText(number,276,y,36,1);
  constexpr int bx=90,bw=178;
  int fill=(int)value*bw/maxValue;
  if (fill>bw) fill=bw;
  gfx->fillRoundRect(bx,y+1,bw,9,3,UI_TRACK);
  if (fill>1) gfx->fillRoundRect(bx,y+1,fill,9,3,color);
}

static void compactProfileStat(int y,const char *label,uint16_t value,uint16_t maxValue,uint16_t color) {
  // The portrait owns x=6..119. Keep every part of this row in the right-hand
  // information column, including the progress track.
  constexpr int x=132, w=176;
  gfx->setTextColor(UI_INK);
  compactText(label,x,y,120,1);
  char number[8]; snprintf(number,sizeof(number),"%u",value);
  compactText(number,276,y,32,1);
  int fill=(int)value*w/maxValue;
  if (fill>w) fill=w;
  gfx->fillRoundRect(x,y+13,w,9,3,UI_TRACK);
  if (fill>1) gfx->fillRoundRect(x,y+13,fill,9,3,color);
}

static void renderCompactProfile() {
  const DexEntry &d=DEX_TBL[pet.speciesId];
  compactScreenTitle("PROFILE");
  gfx->fillRoundRect(6,36,114,158,8,lerp565(d.accent,UI_WHITE,6,8));
  const uint8_t *portrait=thumbs.get(pet.speciesId);
  if (portrait) compactDrawThumb(portrait,Layout::Rect{8,38,110,154},2,false);
  else {
    int flash=flashIdxForDex(pet.speciesId);
    if (flash>=0) drawMap(SPECIES[flash].sprite,32,31,78,2,false);
  }
  const char *nm=pet.nick[0]?pet.nick:dexName(pet.speciesId);
  char head[28]; snprintf(head,sizeof(head),T(S_NAME_FMT),pet.shiny?"*":"",nm,pet.level());
  gfx->setTextColor(d.accent);
  compactButton(Layout::nicknameField,head,UI_WHITE);
  if (pet.nick[0]) {
    char species[32]; snprintf(species,sizeof(species),"(%s)",dexName(pet.speciesId));
    gfx->setTextColor(Layout::secondaryText); compactText(species,132,76,176,1);
  }
  char streak[36]; snprintf(streak,sizeof(streak),T(S_STREAK_FMT),pet.streak,pet.bestStreak);
  gfx->setTextColor(UI_INK); compactText(streak,132,96,176,1);
  compactProfileStat(116,T(S_VIN),pet.bond,100,C565(0xd4,0x52,0x7e));
  const char *berry=!pet.berryKnown?T(S_BERRY_UNK):pet.lovesBerry(0)?T(S_BERRY_RED):pet.lovesBerry(1)?T(S_BERRY_BLUE):T(S_BERRY_GREEN);
  char info[48]; snprintf(info,sizeof(info),T(S_INFO_FMT),berry,(unsigned long)(pet.ageMinutes/1440));
  gfx->setTextColor(UI_INK); compactText(info,132,148,176,1);
  gfx->setTextColor(Layout::secondaryText); compactText(T(S_RENAME_HINT),132,178,176,1);
  compactCardChrome(); gfx->flush();
}

static void renderCompactStats() {
  compactScreenTitle(T(S_BATTLE));
  compactStatRow(48,T(S_STAT_ATK),pet.atkStat(),260,UI_BAR_BAD);
  compactStatRow(74,T(S_STAT_DEF),pet.defStat(),260,0x4C98);
  compactStatRow(100,T(S_STAT_SPE),pet.speStat(),260,UI_BAR_WARN);
  compactStatRow(126,T(S_STAT_WGT),pet.weight,100,0xB3C8);
  compactButton(Layout::trainButton,T(S_TRAIN_STR),UI_BAR_BAD);
  compactCardChrome(); gfx->flush();
}

static void renderCompactMedals() {
  int got=0; for (int i=0;i<MED_COUNT;++i) if (pet.hasMedal(1<<i)) ++got;
  char head[24]; snprintf(head,sizeof(head),T(S_MEDALS_FMT),got,MED_COUNT);
  compactScreenTitle(head);
  for (int i=0;i<MED_COUNT;++i) {
    Layout::Rect r{6+(i%2)*157,38+(i/2)*40,151,34};
    bool earned=pet.hasMedal(1<<i);
    gfx->fillRoundRect(r.x,r.y,r.w,r.h,6,earned?UI_BAR_OK:UI_TRACK);
    gfx->setTextColor(earned?UI_BG_DAY:0x8410);
    compactText(earned?"v":"-",r.x+7,r.y+10,12,1);
    compactText(medalLabel(i),r.x+22,r.y+9,r.w-26,2);
  }
  compactCardChrome(); gfx->flush();
}

static void renderCompactProgress() {
  const DexEntry &d=DEX_TBL[pet.speciesId];
  compactScreenTitle(T(S_PROGRESS));
  char level[12]; snprintf(level,sizeof(level),T(S_LVL_FMT),pet.level());
  gfx->setTextColor(UI_INK); compactText(level,12,48,120,3);
  uint8_t into=pet.ageMinutes%MINUTES_PER_LEVEL;
  gfx->fillRoundRect(12,82,296,16,5,UI_TRACK);
  int fw=292*into/MINUTES_PER_LEVEL;
  if (fw) gfx->fillRoundRect(14,84,fw,12,4,UI_BAR_OK);
  char next[32]; snprintf(next,sizeof(next),T(S_NEXT_LVL_FMT),MINUTES_PER_LEVEL-into,pet.level()+1);
  compactText(next,12,104,296,1);
  gfx->setTextColor(Layout::secondaryText); compactText(T(S_EVO_LABEL),12,128,90,1);
  char evoBuf[32]; const char *evo; uint16_t evoColor=UI_INK;
  if (!d.evolvesTo) evo=T(S_FINAL_FORM);
  else {
    int needed=d.evolveLevel+pet.careMistakes;
    if (pet.level()>=needed) {
      if (pet.lowestStat()>=40) { evo=T(S_EVO_READY); evoColor=UI_BAR_OK; }
      else { evo=T(S_EVO_BLOCKED); evoColor=UI_BAR_BAD; }
    } else { snprintf(evoBuf,sizeof(evoBuf),T(S_EVO_IN_FMT),needed-pet.level()); evo=evoBuf; }
  }
  gfx->setTextColor(evoColor); compactText(evo,104,128,204,1);
  char mistakes[28]; snprintf(mistakes,sizeof(mistakes),T(S_MISTAKES_FMT),pet.careMistakes);
  gfx->setTextColor(pet.careMistakes?UI_BAR_BAD:UI_INK); compactText(mistakes,12,158,296,2);
  compactCardChrome(); gfx->flush();
}

static void renderCompactCard() {
  if (cardPage==0) renderCompactProfile();
  else if (cardPage==1) renderCompactStats();
  else if (cardPage==2) renderCompactMedals();
  else renderCompactProgress();
}

static void compactCardTap(int16_t x,int16_t y) {
  if (Layout::screenBack.contains(x,y)||Layout::cardHome.contains(x,y)) { cardOpen=false; return; }
  for (int i=0;i<4;++i) if (Layout::cardTabs[i].contains(x,y)) { cardPage=i; return; }
  if (cardPage==0&&Layout::nicknameField.contains(x,y)) { openKeyboard(); return; }
  if (cardPage==1&&Layout::trainButton.contains(x,y)) { cardOpen=false; startSack(); }
}

static void renderCompactSettings() {
  compactScreenTitle(T(S_SET_TIME));
  gfx->fillRoundRect(Layout::settingsStatus.x,Layout::settingsStatus.y,Layout::settingsStatus.w,Layout::settingsStatus.h,6,UI_WHITE);
  gfx->setTextColor(rtcAuthoritative()?UI_BAR_OK:Layout::secondaryText);
  uint32_t wall=rtcEpoch(); char status[40];
  snprintf(status,sizeof(status),"%02lu:%02lu  %s",(unsigned long)((wall/3600)%24),
           (unsigned long)((wall/60)%60),rtcTimeStatusLabel());
  compactCentered(status,Layout::settingsStatus,2);
  compactButton(Layout::syncGps,"SYNC GPS",UI_WHITE);
  compactButton(Layout::syncWifi,"SYNC WIFI",UI_WHITE);
  compactButton(Layout::manualTime,"MANUAL",UI_WHITE);
  compactButton(Layout::soundToggle,audioEnabled()?T(S_SND_ON):T(S_SND_OFF),audioEnabled()?UI_BAR_OK:UI_WHITE);
  char lang[16]; snprintf(lang,sizeof(lang),"%s: %s >",T(S_LANG_LABEL),LANG_CODES[gLang]);
  compactButton(Layout::languageToggle,lang,UI_WHITE);
  char wifi[48]; snprintf(wifi,sizeof(wifi),"WIFI: %s%s",rtcWifiEnabled()?"ON ":"OFF",rtcWifiConfigured()?rtcWifiSsid():"NOT SET");
  compactButton(Layout::wifiSettings,wifi,UI_WHITE);
  const char *feedback=rtcSyncFeedback();
  if (feedback) { gfx->setTextColor(UI_BAR_BAD); compactCentered(feedback,Layout::settingsFeedback,1); }
  char ver[24]; snprintf(ver,sizeof(ver),"v%s",FW_VERSION);
  gfx->setTextColor(Layout::secondaryText); compactText(ver,276,226,40,1);
  gfx->flush();
}

static void renderCompactManualClock() {
  compactScreenTitle("SET MANUALLY");
  char timeText[8]; snprintf(timeText,sizeof(timeText),"%02d:%02d",clockH,clockM);
  gfx->setTextColor(UI_INK); compactText(timeText,100,40,120,4);
  compactButton(Layout::hourMinus,"-",UI_WHITE); compactButton(Layout::hourPlus,"+",UI_WHITE);
  compactButton(Layout::minuteMinus,"-",UI_WHITE); compactButton(Layout::minutePlus,"+",UI_WHITE);
  gfx->setTextColor(Layout::secondaryText); compactText(T(S_HOUR),26,134,90,1); compactText(T(S_MIN),216,134,90,1);
  compactButton(Layout::clockApply,"OK",UI_BAR_OK);
  gfx->setTextColor(Layout::secondaryText); compactText(rtcTimeStatusLabel(),12,224,180,1);
  gfx->flush();
}

static void renderCompactWifi() {
  compactScreenTitle("WIFI TIME");
  compactButton(Layout::wifiToggle,rtcWifiEnabled()?"WIFI: ON":"WIFI: OFF",rtcWifiEnabled()?UI_BAR_OK:UI_WHITE);
  char network[44]; snprintf(network,sizeof(network),"SSID: %s",rtcWifiConfigured()?rtcWifiSsid():"NOT SET");
  compactButton(Layout::wifiNetwork,network,UI_WHITE);
  compactButton(Layout::wifiScan,rtcWifiScanning()?"SCANNING":"SCAN",UI_WHITE);
  compactButton(Layout::wifiConnect,"CONNECT",UI_WHITE);
  compactButton(Layout::wifiForget,"FORGET",UI_WHITE);
  compactButton(Layout::wifiSyncTime,"SYNC TIME",UI_BAR_OK);
  const char *feedback=rtcSyncFeedback();
  if (feedback) { gfx->setTextColor(UI_BAR_BAD); compactCentered(feedback,Layout::wifiFeedback,1); }
  gfx->setTextColor(Layout::secondaryText); compactText("Radio turns off after use",66,214,188,1);
  gfx->flush();
}

static void renderCompactNetworks() {
  compactScreenTitle("SELECT NETWORK");
  if (rtcWifiScanning()) {
    gfx->setTextColor(Layout::secondaryText); compactText("SCANNING...",104,108,120,2);
  } else {
    int count=rtcWifiScanCount();
    for (int i=0;i<4;++i) {
      char row[44];
      if (i<count) snprintf(row,sizeof(row),"%s  %d",rtcWifiScanSsid(i),rtcWifiScanRssi(i));
      else snprintf(row,sizeof(row),"-");
      compactButton(Layout::wifiNetworkRow[i],row,i<count?UI_WHITE:UI_TRACK);
    }
  }
  gfx->flush();
}

static void renderCompactWifiPassword() {
  compactScreenTitle("WIFI PASSWORD");
  gfx->setTextColor(Layout::secondaryText); compactText(wifiSelectedSsid,8,36,256,1);
  static const char keys[]="abcdefghijklmnopqrstuvwxyz0123456789.-";
  static const char symbols[]="!@#$%^&*()_+=[]{}:;,.?/-\\|'\"~`<> ";
  for (int i=0;i<42;++i) {
    const auto r=Layout::wifiPasswordKey(i); char key[3]={0}; const char *label=key;
    if (i<38) {
      char c=wifiPasswordMode==2?symbols[i%strlen(symbols)]:keys[i];
      key[0]=wifiPasswordMode==1&&c>='a'&&c<='z'?c-'a'+'A':c;
    } else label=i==38?"aA#":i==39?"<-":i==40?"X":"OK";
    compactButton(r,label,i>=40?UI_BAR_WARN:UI_WHITE);
  }
  char masked[34]; int shown=wifiPasswordLen>32?32:wifiPasswordLen;
  memset(masked,'*',shown); masked[shown]=0;
  gfx->fillRoundRect(8,34,256,18,4,UI_WHITE); gfx->setTextColor(UI_INK); compactText(masked,12,39,248,1);
  gfx->flush();
}

static void renderCompactClock() {
  if (clockPage==1) renderCompactManualClock();
  else if (clockPage==2) renderCompactWifi();
  else if (clockPage==3) renderCompactNetworks();
  else if (clockPage==4) renderCompactWifiPassword();
  else renderCompactSettings();
}

static void compactClockTap(int16_t x,int16_t y) {
  if (Layout::screenBack.contains(x,y)) {
    if (clockPage) clockPage=0; else clockOpen=false;
    return;
  }
  if (clockPage==0) {
    if (Layout::syncGps.contains(x,y)) rtcRequestGpsSync();
    else if (Layout::syncWifi.contains(x,y)) rtcRequestWifiSync();
    else if (Layout::manualTime.contains(x,y)) openManualClock();
    else if (Layout::soundToggle.contains(x,y)) { audioSetEnabled(!audioEnabled()); if (audioEnabled()) sfxPlay(SFX_TAP); }
    else if (Layout::languageToggle.contains(x,y)) { setLang((Lang)((gLang+1)%LANG_COUNT)); applyLangFont(); sfxPlay(SFX_TAP); }
    else if (Layout::wifiSettings.contains(x,y)) clockPage=2;
    return;
  }
  if (clockPage==1) {
    if (Layout::hourMinus.contains(x,y)) clockH=(clockH+23)%24;
  else if (Layout::hourPlus.contains(x,y)) clockH=(clockH+1)%24;
  else if (Layout::minuteMinus.contains(x,y)) clockM=(clockM+59)%60;
  else if (Layout::minutePlus.contains(x,y)) clockM=(clockM+1)%60;
    else if (Layout::clockApply.contains(x,y)) applyClock();
    return;
  }
  if (clockPage==2) {
    if (Layout::wifiToggle.contains(x,y)) rtcWifiSetEnabled(!rtcWifiEnabled());
    else if (Layout::wifiNetwork.contains(x,y)||Layout::wifiScan.contains(x,y)) { if (rtcWifiStartScan()) clockPage=3; }
    else if (Layout::wifiConnect.contains(x,y)) rtcWifiConnectSaved();
    else if (Layout::wifiForget.contains(x,y)) rtcWifiForget();
    else if (Layout::wifiSyncTime.contains(x,y)) rtcRequestWifiSync();
    return;
  }
  if (clockPage==3) {
    if (rtcWifiScanning()) return;
    for (int i=0;i<4&&i<rtcWifiScanCount();++i) if (Layout::wifiNetworkRow[i].contains(x,y)) {
      snprintf(wifiSelectedSsid,sizeof(wifiSelectedSsid),"%s",rtcWifiScanSsid(i));
      wifiPassword[0]=0; wifiPasswordLen=0; wifiPasswordMode=0; clockPage=4; return;
    }
    return;
  }
  if (clockPage==4&&Layout::wifiPasswordArea.contains(x,y)) {
    int col=(x-Layout::wifiPasswordArea.x)/Layout::wifiPasswordCellW;
    int row=(y-Layout::wifiPasswordArea.y)/Layout::wifiPasswordCellH;
    int i=row*Layout::wifiPasswordCols+col;
    if (i<0||i>=42||!Layout::wifiPasswordKey(i).contains(x,y)) return;
    static const char keys[]="abcdefghijklmnopqrstuvwxyz0123456789.-";
    static const char symbols[]="!@#$%^&*()_+=[]{}:;,.?/-\\|'\"~`<> ";
    if (i<38&&wifiPasswordLen<sizeof(wifiPassword)-1) {
      char c=wifiPasswordMode==2?symbols[i%strlen(symbols)]:keys[i];
      wifiPassword[wifiPasswordLen++]=wifiPasswordMode==1&&c>='a'&&c<='z'?c-'a'+'A':c; wifiPassword[wifiPasswordLen]=0;
    } else if (i==38) wifiPasswordMode=(wifiPasswordMode+1)%3;
    else if (i==39&&wifiPasswordLen) wifiPassword[--wifiPasswordLen]=0;
    else if (i==40) clockPage=2;
    else if (i==41) { rtcWifiConnectCandidate(wifiSelectedSsid,wifiPassword); clockPage=2; }
  }
}

static void renderCompactTimePrompt() {
  gfx->fillScreen(UI_BG_DAY); gfx->setTextColor(UI_INK);
  compactText("TIME COULD NOT BE",56,42,208,2); compactText("SYNCHRONIZED",76,66,168,2);
  compactText("Game remains available.",76,98,180,1);
  compactButton(Layout::accept,"SET TIME",UI_BAR_WARN); compactButton(Layout::cancel,"LATER",UI_WHITE); gfx->flush();
}

static bool compactTimePromptTap(int16_t x,int16_t y) {
  if (!timePromptOpen) return false;
  if (Layout::accept.contains(x,y)) { timePromptOpen=false; openManualClock(); }
  else if (Layout::cancel.contains(x,y)) timePromptOpen=false;
  return true;
}

static void renderCompactKeyboard() {
  compactScreenTitle(T(S_NAME));
  gfx->fillRoundRect(8,34,256,22,5,UI_WHITE); gfx->drawRoundRect(8,34,256,22,5,UI_INK);
  gfx->setTextColor(UI_INK); compactText(nameLen?nameBuf:"_",14,39,244,2);
  for (int i=0;i<30;++i) {
    Layout::Rect r=Layout::keyboardKey(i); bool special=i>=28;
    gfx->fillRoundRect(r.x+2,r.y+2,r.w-4,r.h-4,5,special?UI_BAR_WARN:UI_WHITE);
    gfx->drawRoundRect(r.x+2,r.y+2,r.w-4,r.h-4,5,UI_INK);
    char key[3]={0}; const char *label=key;
    if (i<28) { key[0]=KB_KEYS[i]; }
    else label=i==28?"<-":"OK";
    gfx->setTextColor(UI_INK); compactCentered(label,r,2);
  }
  gfx->flush();
}

static void compactKeyboardTap(int16_t x,int16_t y) {
  if (Layout::screenBack.contains(x,y)) { kbOpen=false; return; }
  if (!Layout::keyboardArea.contains(x,y)) return;
  int col=(x-Layout::keyboardArea.x)/Layout::keyboardCellW;
  int row=(y-Layout::keyboardArea.y)/Layout::keyboardCellH;
  int i=row*Layout::keyboardCols+col;
  if (i<0||i>=30||!Layout::keyboardKey(i).contains(x,y)) return;
  if (i==28) { if (nameLen) nameBuf[--nameLen]=0; }
  else if (i==29) { pet.rename(nameBuf); kbOpen=false; }
  else if (nameLen<sizeof(nameBuf)-1) { nameBuf[nameLen++]=KB_KEYS[i]; nameBuf[nameLen]=0; }
}

static void compactDrawThumb(const uint8_t *b,const Layout::Rect &cell,int scale,bool silhouette) {
  uint8_t w=b[0],h=b[1],colors=b[2]; const uint8_t *pal=b+3,*data=pal+colors*2;
  int ox=cell.x+(cell.w-w*scale)/2,oy=cell.y+(cell.h-h*scale)/2;
  for (int row=0;row<h;++row) for (int col=0;col<w;++col) {
    uint8_t idx=data[row*w+col]; if (idx==0xFF) continue;
    uint16_t color=silhouette?INK_K:(uint16_t)(pal[idx*2]|(pal[idx*2+1]<<8));
    gfx->fillRect(ox+col*scale,oy+row*scale,scale,scale,color);
  }
}

static void renderCompactGallery() {
  gfx->fillScreen(UI_BG_DAY);
  if (galleryDetail) {
    const DexEntry &d=DEX_TBL[galleryDetail]; bool registered=pet.isRegistered(galleryDetail);
    char head[40]; snprintf(head,sizeof(head),"N.%03d %s%s",galleryDetail,pet.isShinyRegistered(galleryDetail)?"*":"",registered?dexName(galleryDetail):"???");
    gfx->setTextColor(registered?d.accent:UI_INK); compactText(head,8,8,296,2);
    gfx->drawRect(Layout::detailSprite.x,Layout::detailSprite.y,Layout::detailSprite.w,Layout::detailSprite.h,UI_TRACK);
    if (galleryPmd.loaded) drawPmdActM(galleryPmd,PMD_IDLE,104,198,registered?millis():0,true,!registered,3);
    else { const uint8_t *thumb=thumbs.get(galleryDetail); if (thumb) compactDrawThumb(thumb,Layout::detailSprite,3,!registered); }
    gfx->setTextColor(UI_INK); compactText(registered?dexName(galleryDetail):"???",216,52,96,2);
    char reg[24]; snprintf(reg,sizeof(reg),T(S_POKEDEX_FMT),pet.registeredCount()); compactText(reg,216,92,96,1);
    compactButton(Layout::detailBack,T(S_DETAIL_BACK),UI_WHITE);
    gfx->flush(); return;
  }
  char title[28]; snprintf(title,sizeof(title),T(S_POKEDEX_FMT),pet.registeredCount());
  gfx->setTextColor(UI_INK); compactText(title,8,8,244,2);
  for (int i=0;i<16;++i) {
    int16_t dex=galleryPage*16+i+1; if (dex>151) break;
    Layout::Rect cell=Layout::galleryCell(i);
    gfx->drawRect(cell.x,cell.y,cell.w,cell.h,UI_TRACK);
    const uint8_t *thumb=thumbs.get(dex);
    if (thumb) compactDrawThumb(thumb,cell,1,!pet.isRegistered(dex));
    else { char number[8]; snprintf(number,sizeof(number),"%u",(unsigned)dex); gfx->setTextColor(Layout::secondaryText); compactCentered(number,cell,1); }
    if (pet.isShinyRegistered(dex)) { gfx->setTextColor(UI_BAR_WARN); compactText("*",cell.x+cell.w-10,cell.y+3,8,1); }
  }
  compactButton(Layout::galleryPrev,"<",galleryPage?UI_WHITE:UI_TRACK);
  compactButton(Layout::galleryNext,">",galleryPage<9?UI_WHITE:UI_TRACK);
  char page[8]; snprintf(page,sizeof(page),"%u/10",(unsigned)(galleryPage+1)); gfx->setTextColor(UI_INK); compactText(page,270,154,44,1);
  compactButton(Layout::galleryExit,T(S_BACK),UI_WHITE); gfx->flush();
}

static void compactGalleryTap(int16_t x,int16_t y) {
  if (galleryDetail) { if (Layout::detailBack.contains(x,y)||Layout::screenBack.contains(x,y)) { galleryDetail=0; galleryPmd.unload(); galleryDirty=true; } return; }
  if (Layout::galleryExit.contains(x,y)) { galleryOpen=false; galleryPmd.unload(); return; }
  if (Layout::galleryPrev.contains(x,y)) { if (galleryPage) --galleryPage; return; }
  if (Layout::galleryNext.contains(x,y)) { if (galleryPage<9) ++galleryPage; return; }
  if (!Layout::galleryGrid.contains(x,y)) return;
  int col=(x-Layout::galleryGrid.x)/Layout::galleryCellW,row=(y-Layout::galleryGrid.y)/Layout::galleryCellH;
  int16_t dex=galleryPage*16+row*4+col+1; if (dex>151) return;
  galleryDetail=dex; galleryPmd.load(dex,pet.isShinyRegistered(dex));
}

static void renderCompactSack() {
  uint32_t now=millis(); gfx->fillScreen(UI_BG_DAY); compactButton(Layout::sackExit,"X",UI_WHITE);
  if (sackOverUntil) {
    if (!timeLeft(sackOverUntil)) { sackOpen=false; return; }
    char hits[24]; snprintf(hits,sizeof(hits),T(S_HITS_FMT),sackHits);
    char gain[24]; snprintf(gain,sizeof(gain),T(S_STR_GAIN_FMT),sackGain);
    gfx->setTextColor(UI_INK); compactText(hits,24,62,272,4);
    gfx->setTextColor(UI_BAR_BAD); compactText(gain,56,122,208,3);
    gfx->setTextColor(sackNewHi&&sackHits?UI_BAR_WARN:UI_INK);
    if (sackNewHi&&sackHits) compactText(T(S_NEW_RECORD),70,172,180,2);
    else { char record[24]; snprintf(record,sizeof(record),T(S_RECORD_FMT),pet.strHi); compactText(record,70,172,180,2); }
    gfx->flush(); return;
  }
  if (!timeLeft(sackUntil)) {
    sackNewHi=sackHits>pet.strHi; sackGain=pet.trainStrength(sackHits);
    sfxPlay(sackNewHi?SFX_MEDAL:SFX_PLAY); sackOverUntil=now+3500; gfx->flush(); return;
  }
  sackShake*=0.84f; int off=(int)(sackShake*sinf(now*0.05f)),sx=160+off;
  gfx->drawRoundRect(Layout::sackTarget.x,Layout::sackTarget.y,Layout::sackTarget.w,Layout::sackTarget.h,8,UI_TRACK);
  gfx->fillRect(157,20,6,30,UI_INK); gfx->fillRect(sx-4,40,8,18,UI_INK);
  gfx->fillRoundRect(sx-38,54,76,116,24,C565(0xb5,0x3a,0x3a)); gfx->drawRoundRect(sx-38,54,76,116,24,UI_INK);
  char count[8]; snprintf(count,sizeof(count),"%u",sackHits); gfx->setTextColor(UI_INK); compactText(count,18,60,72,4);
  compactText(T(S_HIT_FAST),8,116,100,1);
  uint32_t left=sackUntil-now; int width=296*(uint32_t)left/10000;
  gfx->fillRoundRect(12,190,296,14,4,UI_TRACK); if (width>1) gfx->fillRoundRect(12,190,width,14,4,UI_BAR_OK);
  gfx->flush();
}

static int compactGameCoord(float logical) { return (int)(logical*240.0f/466.0f); }

static void renderCompactGame() {
  bool night=sceneHour()<6||sceneHour()>=20; uint16_t ink=night?UI_INK_NIGHT:UI_INK;
  gfx->fillScreen(night?UI_BG_NIGHT:UI_BG_DAY);
  gfx->fillCircle(120,120,106,night?C565(0x1e,0x26,0x46):C565(0x8f,0xc8,0xea));
  gfx->drawCircle(120,120,106,ink); compactFill(Layout::gameHud,night?UI_BG_NIGHT:UI_BG_DAY);
  if (gameOverUntil) {
    if (!timeLeft(gameOverUntil)) { gameOpen=false; return; }
    char score[24]; snprintf(score,sizeof(score),T(S_SCORE_FMT),gameScore);
    gfx->setTextColor(ink); compactText(score,20,74,200,3);
    char record[24]; snprintf(record,sizeof(record),T(S_RECORD_FMT),pet.gameHi); compactText(gameNewHi?T(S_NEW_RECORD):record,24,124,192,2);
    compactButton(Layout::gameExit,T(S_BACK),UI_WHITE); gfx->flush(); return;
  }
  stepGame();
  char score[8]; snprintf(score,sizeof(score),"%u",gameScore); gfx->setTextColor(ink); compactText(score,252,20,58,4);
  char record[16]; snprintf(record,sizeof(record),T(S_REC_FMT),pet.gameHi); compactText(record,246,62,68,1);
  for (int i=0;i<3;++i) { int cy=96+i*24; if (i<3-gameMisses) gfx->fillCircle(280,cy,6,UI_BAR_BAD); else gfx->drawCircle(280,cy,6,UI_TRACK); }
  const uint8_t *thumb=thumbs.get(pet.speciesId);
  if (thumb) { Layout::Rect petCell{compactGameCoord(gamePetX)-22,184,44,42}; compactDrawThumb(thumb,petCell,1,false); }
  uint32_t elapsed=millis()-hitTime;
  if (hitTime&&elapsed<260) gfx->drawCircle(compactGameCoord(hitX),compactGameCoord(hitY),12+(int)(elapsed/12),UI_BAR_WARN);
  int bx=compactGameCoord(ballX),by=compactGameCoord(ballY); drawMap(SPR_ICON_PLAY,16,bx-16,by-16,2,false);
  compactButton(Layout::gameExit,T(S_BACK),UI_WHITE); gfx->flush();
}

static void compactGameTap(int16_t x,int16_t y) {
  if (Layout::gameExit.contains(x,y)) { gameOpen=false; return; }
  if (!Layout::gameViewport.contains(x,y)||gameOverUntil) return;
  int logicalX=x*466/240,logicalY=y*466/240;
  gameTap(logicalX,logicalY);
}

#endif
