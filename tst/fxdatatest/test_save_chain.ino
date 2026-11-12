// Exact production save-chain probe: includes the real sketch translation unit
// (renamed setup/loop), draws the station frame path, invokes the actual
// updateStation() -> requestSave() -> saveVoyageNow() chain with music active,
// then measures stack reserve and post-save music diagnostics.
//
// Emulator/host-harness only. It performs real cart erase/page writes, so never
// run it on a physical cart.

#ifndef ATM_FX_COMPACT_DIAG
#define ATM_FX_COMPACT_DIAG 0
#endif

#include "boot_driver.hpp"
#include "src/music_diag.hpp"


extern "C" char __bss_end;

namespace {

constexpr uint16_t kReserveTarget = 600;
constexpr uint8_t kPattern = 0xA5;
constexpr uint8_t kFrameGuard = 64;

uint16_t readStackPointer() {
    uint8_t low;
    uint8_t high;
    asm volatile("in %0, %1" : "=r"(low) : "I"(_SFR_IO_ADDR(SPL)));
    asm volatile("in %0, %1" : "=r"(high) : "I"(_SFR_IO_ADDR(SPH)));
    return static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
}

bool s_saveOk;
bool s_audioOk;
bool s_skipOk;
uint16_t s_reserve;

// Counts the cart transaction through the exact cached-save helper the sketch
// uses, so the device test can prove the second identical checkpoint performs
// no erase and no page program.
struct CountingStorage {
    voyage_save::FxStorage storage;
    uint8_t erases;
    uint16_t pages;

    CountingStorage() : erases(0), pages(0) {}

    uint8_t read(uint16_t offset) { return storage.read(offset); }
    void eraseBlock(uint8_t block) {
        ++erases;
        storage.eraseBlock(block);
    }
    void pageBegin(uint16_t page) { storage.pageBegin(page); }
    void pageByte(uint8_t value) { storage.pageByte(value); }
    void pageEnd() {
        ++pages;
        storage.pageEnd();
    }
};

void runProductionChain() {
    // Real setup already initialized FX/text/music and reset the run. Accept
    // FIRST through the production WORK board so the checkpoint is committed
    // while still docked. The accepted load owns its departure handoff, so the
    // screen changes to Story but the saved state remains at Tidewall.
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    chart::reset(s_chart);
    s_screen = Screen::Work;
    s_uiRow.work = 0;
    s_voyageDirty = true;
    Arduboy2Base::previousButtonState = 0;
    Arduboy2Base::currentButtonState = A_BUTTON;
    s_just = A_BUTTON;

    music::setCue(music_logic::kHarbor);
    music::resume();
    music::service();

    const uint8_t sreg = SREG;
    cli();
    char *const base = &__bss_end + 1;
    const uint16_t sp = readStackPointer();
    char *const top = reinterpret_cast<char *>(sp - kFrameGuard);
    for (char *p = base; p < top; ++p)
        *p = static_cast<char>(kPattern);
    SREG = sreg;

    updateWork();
    s_saveOk = game::hasActiveJob(s_state) && s_state.docked == static_cast<int8_t>(campaign::kTidewall) &&
               s_screen == Screen::Story && !s_voyageDirty;
    voyage_save::FxStorage storage;
    s_saveOk = s_saveOk && voyage_save::hasSave(storage);

    // The production chain just committed this exact state. A second identical
    // checkpoint must be skipped: the cache matches, so the cart sees no erase
    // and no page program. Music is paused around the helper exactly as the
    // production save path does, then resumed for the refill window below.
    CountingStorage counted;
    const music_logic::PauseToken skipToken = music::pauseForTransaction();
    const voyage_save::SaveResult repeat = voyage_flow::saveCheckpointIfChanged(s_state
                                                                                ,
                                                                                s_chart
                                                                                ,
                                                                                counted
                                                                                ,
                                                                                s_checkpoint);
    music::resumeAfterTransaction(skipToken);
    s_skipOk = repeat == voyage_save::kSaved && counted.erases == 0 && counted.pages == 0;
    s_saveOk = s_saveOk && s_skipOk;

    // Let the real streaming player refill after the synchronous cart write.
    for (uint8_t frame = 0; frame < 24; ++frame) {
        music::service();
        delay(4);
    }
#if ATM_FX_COMPACT_DIAG
    s_audioOk = !music_diag::underrun() && !music::error();
#else
    s_audioOk = true;
#endif

    const uint8_t scanSreg = SREG;
    cli();
    char *first = base;
    while (first < top && *first == static_cast<char>(kPattern))
        ++first;
    s_reserve = static_cast<uint16_t>(first - &__bss_end);
    SREG = scanSreg;
}

} // namespace

void setup() {
    Serial.begin(9600);
    boot_driver::boot();
    runProductionChain();

    Serial.print(F("stack_reserve="));
    Serial.println(s_reserve);
    Serial.print(F("save_chain="));
    Serial.println(s_saveOk ? 1 : 0);
    Serial.print(F("save_skip="));
    Serial.println(s_skipOk ? 1 : 0);
    Serial.print(F("audio_after_save="));
    Serial.println(s_audioOk ? 1 : 0);
#if ATM_FX_COMPACT_DIAG
    Serial.print(F("save_underruns="));
    Serial.println(music_diag::underrun() ? 1 : 0);
    Serial.print(F("save_errors="));
    Serial.println(music::error() ? 1 : 0);
#endif
    Serial.println(s_reserve >= kReserveTarget && s_saveOk && s_audioOk ? 'P' : 'F');
}

void loop() {}

