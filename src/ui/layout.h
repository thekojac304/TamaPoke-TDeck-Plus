#pragma once
#include "../board/board_select.h"
namespace Layout {
struct Rect {
 int x, y, w, h;
 constexpr bool contains(int px, int py) const {
  return px >= x && py >= y && px < x+w && py < y+h;
 }
};
#if TAMAPOKE_TDECK_PLUS
#include "layout_landscape320.h"
#else
#include "layout_round466.h"
#endif
}
