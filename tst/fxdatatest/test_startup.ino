// Verify startup/title pixels against the general blitter and time the real
// boot-only introduction. Uses the existing cart and framebuffer, no extra RAM.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <util/crc16.h>
#include "src/startup_view.hpp"
#include "src/voyage_view.hpp"

Arduboy2Base arduboy;

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
    // The compact title sheet has four boot states x two selected rows with
    // HAULER fixed; the replace prompt is one frame because it only exists when
    // a valid save is present.
    for (uint8_t statusIndex = 0; statusIndex < 4; ++statusIndex) {
        for (uint8_t selected = 0; selected < 2; ++selected) {
            const uint16_t frame = static_cast<uint16_t>(statusIndex * 2u + selected);
            background(0xA5);
            Blit::draw(0, 0, TITLE_MENU_WIDTH, TITLE_MENU_HEIGHT, title_menu + 2, frame, Blit::OVERWRITE);
            const uint16_t expected = checksum();
            voyage_flow::BootStatus status = {};
            status.state = static_cast<voyage_flow::BootState>(statusIndex);
            background(0x5A);
            voyage_view::drawTitle(status, selected, voyage_view::kTitleMenu);
            ok = (checksum() == expected) && ok;
            ++cases;
        }
    }
    background(0xA5);
    Blit::draw(0, 0, TITLE_REPLACE_WIDTH, TITLE_REPLACE_HEIGHT, title_replace + 2, 0, Blit::OVERWRITE);
    const uint16_t replaceExpected = checksum();
    voyage_flow::BootStatus replaceStatus = {};
    replaceStatus.state = voyage_flow::kBootValid;
    background(0x5A);
    voyage_view::drawTitle(replaceStatus, 0, voyage_view::kTitleConfirmReplace);
    ok = (checksum() == replaceExpected) && ok;
    ++cases;
    // Drive the production startup sequence without a physical button. The
    // script holds A through the logo, releases it, waits on the game splash,
    // then gives one fresh press. It proves the held logo input cannot skip the
    // splash, that the fresh release is consumed before returning, and that the
    // splash services its poll on every waiting frame.
    struct ScriptedPoll {
        uint16_t ticks = 0;
        uint16_t held = 0;
        uint16_t serviced = 0;
        uint8_t buttons() const {
            if (ticks < 64)
                return A_BUTTON;   // held through and just past the logo
            if (ticks < 160)
                return 0;          // released; splash must keep waiting
            if (ticks < 162)
                return A_BUTTON;   // fresh press
            return 0;
        }
        void delay() {
            ++ticks;
        }
        void service() {
            ++serviced;
        }
        void waitNoButtons() {
            ++held;
        }
    } poll;
    startup_view::run(poll);
    ok = poll.ticks >= 160 && poll.held == 1 && poll.serviced >= 160 && ok;
    Serial.print(F("startup pixels=")); Serial.print(cases);
    Serial.print(F(" startup_ticks=")); Serial.print(poll.ticks);
    Serial.print(F(" releases=")); Serial.print(poll.held);
    Serial.print(F(" serviced=")); Serial.println(poll.serviced);
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}
