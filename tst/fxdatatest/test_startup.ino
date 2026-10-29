// Verify startup/title pixels against the general blitter and time the real
// boot-only introduction. Uses the existing cart and framebuffer, no extra RAM.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <util/crc16.h>
#include "src/startup_view.hpp"
#include "src/voyage_view.hpp"

Arduboy2Base arduboy;

#if SH_FEATURE_SAVE_VOYAGE && SH_FEATURE_MULTI_HULL && !SH_DEV_MODE
uint16_t checksum() {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < 1024; ++i)
        crc = _crc_ccitt_update(crc, Arduboy2Base::sBuffer[i]);
    return crc;
}

void background(uint8_t value) {
    memset(Arduboy2Base::sBuffer, value, 1024);
}

void setup() {
    Serial.begin(9600);
    arduboy.boot();
    FX::begin(FX_DATA_PAGE);
    bool ok = true;
    uint8_t cases = 0;
    for (uint8_t frame = 0; frame < STARTUP_FRAMES; ++frame) {
        background(0xA5);
        Blit::draw(0, 0, STARTUP_WIDTH, STARTUP_HEIGHT, startup + 2, frame, Blit::OVERWRITE);
        const uint16_t expected = checksum();
        background(0x5A);
        panel_view::draw(startup, frame, STARTUP_HEIGHT);
        ok = (checksum() == expected) && ok;
        ++cases;
    }
    // Title and replacement panels changed readers to recover program flash.
    // Compare every generated variant, including the reserved status frame.
    for (uint8_t frame = 0; frame < TITLE_MENU_FRAMES; ++frame) {
        background(0xA5);
        Blit::draw(0, 0, TITLE_MENU_WIDTH, TITLE_MENU_HEIGHT, title_menu + 2, frame, Blit::OVERWRITE);
        const uint16_t expected = checksum();
        voyage_flow::BootStatus status = {};
        status.state = static_cast<voyage_flow::BootState>(frame / 9);
        background(0x5A);
        voyage_view::drawTitle(status, frame % 3, (frame % 9) / 3, voyage_view::kTitleMenu);
        ok = (checksum() == expected) && ok;
        ++cases;
    }
    for (uint8_t frame = 0; frame < TITLE_REPLACE_FRAMES; ++frame) {
        background(0xA5);
        Blit::draw(0, 0, TITLE_REPLACE_WIDTH, TITLE_REPLACE_HEIGHT, title_replace + 2, frame, Blit::OVERWRITE);
        const uint16_t expected = checksum();
        voyage_flow::BootStatus status = {};
        status.state = static_cast<voyage_flow::BootState>(frame);
        background(0x5A);
        voyage_view::drawTitle(status, 0, 0, voyage_view::kTitleConfirmReplace);
        ok = (checksum() == expected) && ok;
        ++cases;
    }
    const uint32_t started = millis();
    startup_view::show();
    const uint32_t elapsed = millis() - started;
    ok = elapsed >= 3250 && elapsed < 4000 && ok;
    Serial.print(F("startup pixels=")); Serial.print(cases);
    Serial.print(F(" elapsed_ms=")); Serial.println(elapsed);
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}
#else
void setup() { Serial.begin(9600); Serial.println(F("P")); }
void loop() {}
#endif
