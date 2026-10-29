// Emulator-only runtime save-page suite. Erases the alternate cart page; never
// run on physical hardware. Shares the normal save fixtures.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "test_save_shared.hpp"


void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    const bool ok = runSaveTests(true);
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}
