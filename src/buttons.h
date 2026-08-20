#pragma once

// Hardware bring-up test for two momentary buttons tied to ground
// (BUTTON1_PIN / BUTTON2_PIN in config.h). Presses/releases are just
// logged to Serial — no UI or WLED action wired up yet.

void buttons_init();
void buttons_update();
