#pragma once

#include <stdint.h>

#include "features.hpp"

// Shared renderer for generated 128x64 story panels. The panel image already
// contains every fixed label and the selected-row bar, so one Blit call replaces
// repeated cursor/string drawing. Device-only: the host tests exercise the pure
// card/row contracts.
#if (SH_FEATURE_CAMPAIGN || SH_FEATURE_SAVE_VOYAGE) && defined(__AVR__)

#include "cart_service.hpp"
#include "common.hpp"
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
    for (uint16_t offset = 0; offset < bytes; offset = static_cast<uint16_t>(offset + 16)) {
        SH_CART_SERVICE();
        FX::readDataBytes(source + offset, dest + offset, 16);
    }
}

inline void draw(uint24_t image) {
    draw(image, 0, kHeight);
}

}   // namespace panel_view

#endif
