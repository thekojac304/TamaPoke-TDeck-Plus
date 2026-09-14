// A tall scene on the left fits every current PMD frame at 1x.
constexpr bool compact = true;
// T-Deck LCD-specific text tones. Keep these local to the compact UI so the
// Waveshare AMOLED palette and appearance remain unchanged.
constexpr uint16_t secondaryText = 0x630C; // #606064: readable on UI_BG_DAY
constexpr int centerX = 104, centerY = 120, petY = 112, ground = 194;
constexpr int swipe = 45;
constexpr Rect scene{0, 24, 208, 174};
constexpr Rect header{0,0,320,24}, sidebar{208,24,112,176}, toolbar{0,200,320,40};
constexpr Rect needs[4] = {{216,30,96,20},{216,58,96,20},{216,86,96,20},{216,114,96,20}};
constexpr Rect status{216,142,100,8}, eggMessage{4,206,312,28}, eggDetails{216,40,96,44};
constexpr int wanderMin=70, wanderSpan=68;
constexpr Rect starter[3] = {{12,40,296,56},{12,104,296,56},{12,168,296,56}};
constexpr Rect care[4] = {{0,200,80,40},{80,200,80,40},{160,200,80,40},{240,200,80,40}};
constexpr Rect food[4] = {{16,140,72,48},{88,140,72,48},{160,140,72,48},{232,140,72,48}};
constexpr Rect prompt{12,48,296,144};
constexpr Rect accept{24,94,272,40}, cancel{24,144,272,40};
constexpr Rect progression{216,150,96,44};
// Home navigation: these same rectangles are rendered and hit-tested.
constexpr Rect homeProfile{0,0,40,24}, homeDex{224,0,48,24}, homeSettings{272,0,48,24};

// Shared secondary-screen chrome.
constexpr Rect screenBack{272,0,48,32};
constexpr Rect cardTabs[4] = {{0,204,64,36},{64,204,64,36},{128,204,64,36},{192,204,64,36}};
constexpr Rect cardHome{256,204,64,36};
constexpr Rect nicknameField{128,38,184,34};
constexpr Rect trainButton{52,158,216,38};

// Settings landing page: time source owns a full row, separate from controls.
constexpr Rect settingsStatus{12,34,296,26};
constexpr Rect syncGps{12,68,92,36}, syncWifi{114,68,92,36}, manualTime{216,68,92,36};
constexpr Rect soundToggle{12,112,140,36}, languageToggle{168,112,140,36};
constexpr Rect wifiSettings{12,156,296,36}, settingsFeedback{12,200,296,24};
// Manual clock page.
constexpr Rect hourMinus{12,88,52,44}, hourPlus{68,88,52,44};
constexpr Rect minuteMinus{200,88,52,44}, minutePlus{256,88,52,44};
constexpr Rect clockOnlyApply{12,166,142,42}, clockCatchup{166,166,142,42};
constexpr Rect manualSource{12,216,200,18};
constexpr Rect catchupInfo{12,38,296,104}, catchupApply{24,148,272,38}, catchupCancel{24,194,272,38};
// Minimal Wi-Fi/NTP page.
constexpr Rect wifiToggle{12,40,92,36}, wifiNetwork{114,40,194,36};
constexpr Rect wifiScan{12,84,92,36}, wifiConnect{114,84,92,36}, wifiForget{216,84,92,36};
constexpr Rect wifiSyncTime{12,128,296,36}, wifiFeedback{12,174,296,24};
constexpr Rect wifiNetworkRow[4]={{12,40,296,36},{12,82,296,36},{12,124,296,36},{12,166,296,36}};
constexpr Rect wifiPasswordArea{2,54,315,186};
constexpr int wifiPasswordCols=7,wifiPasswordRows=6,wifiPasswordCellW=45,wifiPasswordCellH=31;
constexpr Rect wifiPasswordKey(int i) {
 return Rect{wifiPasswordArea.x+(i%wifiPasswordCols)*wifiPasswordCellW,
             wifiPasswordArea.y+(i/wifiPasswordCols)*wifiPasswordCellH,
             wifiPasswordCellW,wifiPasswordCellH};
}

// Touch nickname keyboard: six columns by five rows fill the lower 180 px.
constexpr Rect keyboardArea{1,60,318,180};
constexpr int keyboardCols=6, keyboardRows=5, keyboardCellW=53, keyboardCellH=36;
constexpr Rect keyboardKey(int i) {
 return Rect{keyboardArea.x+(i%keyboardCols)*keyboardCellW,
             keyboardArea.y+(i/keyboardCols)*keyboardCellH,
             keyboardCellW,keyboardCellH};
}

// Pokédex: 16 cells plus a landscape navigation rail.
constexpr Rect galleryHeader{0,0,260,32}, galleryGrid{0,32,260,208};
constexpr int galleryCols=4, galleryRows=4, galleryCellW=65, galleryCellH=52;
constexpr Rect galleryCell(int i) {
 return Rect{galleryGrid.x+(i%galleryCols)*galleryCellW,
             galleryGrid.y+(i/galleryCols)*galleryCellH,
             galleryCellW,galleryCellH};
}
constexpr Rect galleryPrev{264,36,52,48}, galleryNext{264,94,52,48}, galleryExit{264,188,52,48};
constexpr Rect detailSprite{0,32,208,176}, detailInfo{208,32,112,156}, detailBack{208,188,112,48};

// Training layouts. The ball game keeps 466x466 logical physics in this square.
constexpr Rect sackExit{272,0,48,32}, sackTarget{72,38,176,136};
constexpr Rect gameViewport{0,0,240,240}, gameHud{240,0,80,240}, gameExit{244,188,72,44};
constexpr int fitScale(int w, int h, int preferred) {
 int s = preferred;
 while (s > 1 && (w*s > scene.w || h*s > scene.h)) --s;
 return s < 1 ? 1 : s;
}
