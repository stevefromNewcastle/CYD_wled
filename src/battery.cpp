#include "battery.h"

void battery_init() {
    // No-op — no battery hardware on this board.
}

int battery_get_percent() {
    return -1;
}
