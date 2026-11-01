// Sub-hauler — Arduboy FX fixed release entry.
//
// The shared application lives in src/app.hpp. This file owns the USB-free
// entry macro and the two Arduino entry points only.

#include "src/app.hpp"

ARDUBOY_NO_USB

void setup() {
    applicationBoot();
}

void loop() {
    music::service();
    if (!arduboy.nextFrame()) {
        music::service();
        return;
    }
    arduboy.pollButtons();
    s_just = static_cast<uint8_t>(Arduboy2Base::currentButtonState & ~Arduboy2Base::previousButtonState);
    s_present = true;

    switch (s_screen) {
    case Screen::Title:
        updateTitle();
        break;
    case Screen::SaveFailed:
        updateSaveFailed();
        break;
    case Screen::Pause:
        updatePause();
        break;
    case Screen::Station:
        updateStation();
        break;
    case Screen::Work:
        updateWork();
        break;
    case Screen::Story:
        updateStory();
        break;
    case Screen::Ending:
        updateEnding();
        break;
    case Screen::Chart:
        updateChart();
        break;
    case Screen::Pilot:
        updateFlight();
        break;
    default:
        break;
    }

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction. A static screen with no input
    // edge clears s_present and keeps the previous panel image; the buffer is
    // already clear from the last display.
    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction. A static screen with no input
    // edge clears s_present and keeps the previous panel image; the buffer is
    // already clear from the last display.
    if (s_present)
        FX::display(CLEAR_BUFFER);

    music::service();
}
