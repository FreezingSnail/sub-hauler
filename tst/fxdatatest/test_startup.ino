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
    // The compact title sheet has two boot states x two selected rows with
    // HAULER fixed. The replace prompt is one frame because it only exists when
    // a written save is present.
    for (uint8_t statusIndex = 0; statusIndex < 2; ++statusIndex) {
        for (uint8_t selected = 0; selected < 2; ++selected) {
            const uint16_t frame = static_cast<uint16_t>(statusIndex * 2u + selected);
            background(0xA5);
            Blit::draw(0, 0, TITLE_MENU_WIDTH, TITLE_MENU_HEIGHT, title_menu + 2, frame, Blit::OVERWRITE);
            const uint16_t expected = checksum();
            const voyage_flow::BootState status = static_cast<voyage_flow::BootState>(statusIndex);
            background(0x5A);
            voyage_view::drawTitle(status, selected, voyage_view::kTitleMenu);
            ok = (checksum() == expected) && ok;
            ++cases;
        }
    }
    background(0xA5);
    Blit::draw(0, 0, TITLE_REPLACE_WIDTH, TITLE_REPLACE_HEIGHT, title_replace + 2, 0, Blit::OVERWRITE);
    const uint16_t replaceExpected = checksum();
    background(0x5A);
    voyage_view::drawTitle(voyage_flow::kBootWritten, 0, voyage_view::kTitleConfirmReplace);
    ok = (checksum() == replaceExpected) && ok;
    ++cases;
    // Drive the production startup sequence without a physical button.
    // First: any input advances the brand screen immediately, then the held
    // input must be released before the game splash accepts a fresh press.
    struct SkipPoll {
        uint16_t ticks = 0;
        uint16_t held = 0;
        uint16_t serviced = 0;
        uint8_t buttons() const {
            if (ticks < 2)
                return A_BUTTON;   // pressed on the brand screen
            if (ticks < 8)
                return 0;          // released; splash must keep waiting
            if (ticks < 10)
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
    } skip;
    startup_view::run(skip);
    ok = skip.ticks <= 12 && skip.held == 1 && skip.serviced >= 8 && ok;

    // Second: no input lets the three-second brand timer expire. A held input
    // at the splash boundary is still consumed by the release policy.
    struct TimeoutPoll {
        static constexpr uint16_t kPressTick = startup_view::kBrandFrames + 12;
        uint16_t ticks = 0;
        uint16_t held = 0;
        uint16_t serviced = 0;
        uint8_t buttons() const {
            if (ticks < startup_view::kBrandFrames)
                return 0;          // let the brand timer expire
            if (ticks < startup_view::kBrandFrames + 2)
                return A_BUTTON;   // held as the splash appears
            if (ticks < kPressTick)
                return 0;          // release arms the splash
            if (ticks < kPressTick + 2)
                return A_BUTTON;   // fresh press dismisses the splash
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
    } timeout;
    startup_view::run(timeout);
    ok = timeout.ticks >= TimeoutPoll::kPressTick && timeout.held == 1 &&
         timeout.serviced >= TimeoutPoll::kPressTick && ok;

    Serial.print(F("startup pixels=")); Serial.print(cases);
    Serial.print(F(" startup_skip_ticks=")); Serial.print(skip.ticks);
    Serial.print(F(" startup_timeout_ticks=")); Serial.print(timeout.ticks);
    Serial.print(F(" releases=")); Serial.print(skip.held + timeout.held);
    Serial.print(F(" serviced=")); Serial.println(skip.serviced + timeout.serviced);
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}
