#pragma once

#include <stdint.h>


// Shared renderer for generated 128x64 story/menu panels. The panel image
// already contains every fixed label and the selected-row bar, so one Blit call
// replaces repeated cursor/string drawing. Device-only: the host tests exercise
// the pure card/row contracts.
#if defined(__AVR__)

#include "cart_service.hpp"
#include "common.hpp"
#include "generated/menu_sprites.hpp"
#include "generated/panel_sprites.hpp"

namespace panel_view {

constexpr uint8_t kWidth = 128;
constexpr uint8_t kHeight = 64;

// Every panel is opaque and screen-wide, so no clipping, scaling or mask is
// needed: contiguous cart reads expand straight into the framebuffer. The
// generic Blit path stays for pilots and small overlays. `height` selects the
// panel row block (64 for full screens, 44 for the WORK carrying strip).
inline void draw(uint24_t image, uint16_t frame, uint8_t height) {
    const uint16_t bytes = static_cast<uint16_t>(kWidth) * (height / 8);
    const uint24_t source = image + 2 + static_cast<uint24_t>(frame) * bytes;
    uint8_t *dest = Arduboy2Base::sBuffer;
    // 128-byte bursts keep each cart transaction short; the foreground service
    // hook runs between bursts so a 1,024-byte panel cannot stretch the audio
    // refill window past its deadline.
    for (uint16_t offset = 0; offset < bytes; offset = static_cast<uint16_t>(offset + 128)) {
        cartService();
        FX::readDataBytes(source + offset, dest + offset, 128);
    }
}

inline void draw(uint24_t image) {
    draw(image, 0, kHeight);
}

// The existing clipped blitter handles the seven-row hint and preserves y56.
// Reuse it instead of keeping another page shifter and scratch buffer.
inline void drawHint7(uint24_t image, uint16_t frame) {
    cartService();
    Blit::draw(0, 57, kWidth, 7, image + 2, frame, Blit::OVERWRITE);
}

}   // namespace panel_view

#endif
