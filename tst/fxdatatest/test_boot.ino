// Device smoke test: boots the FX cart, checks the generated sprite table, and
// prints exactly one P/F marker for the Ardens serial harness.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "src/generated/sprites.hpp"

Arduboy2 arduboy;

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    bool ok = HERO_WIDTH == 16 && HERO_HEIGHT == 16 && HERO_FRAMES == 3 && HERO_MASKED;

    FX::seekData(hero);
    const uint8_t width = FX::readPendingUInt8();
    const uint8_t height = FX::readEnd();
    ok = ok && width == HERO_WIDTH && height == HERO_HEIGHT;

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}
