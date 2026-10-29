// Device test: single-slot fresh voyage checkpoint in the Arduboy FX save
// region. Runs under Ardens (or hardware) with the generated 4 KiB partition.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "test_save_shared.hpp"


void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    const bool ok = runSaveTests(false);
    Serial.println(ok ? F("P") : F("F"));
}

void loop() {}
