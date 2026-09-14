// A tall scene on the left fits every current PMD frame at 1x.
constexpr bool compact = true;
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
constexpr int fitScale(int w, int h, int preferred) {
 int s = preferred;
 while (s > 1 && (w*s > scene.w || h*s > scene.h)) --s;
 return s < 1 ? 1 : s;
}
