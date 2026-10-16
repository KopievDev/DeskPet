#pragma once

#include <stdint.h>

#include "MoodPalette.h"

// Stylized Furious halo for the 320x170 IPS display. Draws only a few
// RGB565 rounded-rectangle outlines behind each angry eye. A real blur or
// alpha framebuffer would be much more expensive and unnecessary here.
namespace deskpet_fury_glow {

constexpr uint32_t kPulsePeriodMs = 1200u;
constexpr uint32_t kIntroFlashMs = 320u;

struct Halo {
    uint8_t outer;
    uint8_t middle;
    uint8_t inner;
};

// A steady breathing pulse plus a brief flare on entering Furious.
// Multiplication by "strength" keeps the halo in step with the existing
// 0..255 eye-color transition, including a smooth fade back to Neutral.
inline Halo alpha(uint32_t elapsedMs, uint8_t strength) {
    if (strength == 0u) return {0u, 0u, 0u};
    const uint32_t t = elapsedMs % kPulsePeriodMs;
    const uint32_t wave = t < 440u
        ? (t * 255u) / 440u
        : ((kPulsePeriodMs - t) * 255u) / (kPulsePeriodMs - 440u);
    uint32_t flash = 0u;
    if (elapsedMs < kIntroFlashMs) {
        // Triangle flash with peak at 160 ms: no hard single-frame jump.
        flash = elapsedMs <= 160u
            ? (elapsedMs * 255u) / 160u
            : ((kIntroFlashMs - elapsedMs) * 255u) / 160u;
    }
    const uint32_t glow = 64u + (wave * 60u) / 255u +
                          (flash * 55u) / 255u;
    const uint32_t scaled = (glow * strength) / 255u;
    return {
        static_cast<uint8_t>(scaled / 4u),
        static_cast<uint8_t>((scaled * 2u) / 5u),
        static_cast<uint8_t>((scaled * 3u) / 5u)
    };
}

// Callback signature: stroke(x, y, w, h, radius, color).
// The caller draws the eye fill over these rings afterwards so they cannot
// obscure a pupil. The 14px outer ring stays above the bottom HUD at y=153.
template <typename Stroke>
void draw(int left, int top, int width, int height, int radius,
          uint32_t elapsedMs, uint8_t strength,
          uint16_t background, Stroke stroke) {
    if (strength < 4u || width <= 0 || height <= 0) return;
    const Halo halo = alpha(elapsedMs, strength);
    const uint8_t magnitudes[] = {halo.outer, halo.middle, halo.inner};
    const int offsets[] = {13, 9, 5};

    for (uint8_t ring = 0; ring < 3u; ++ring) {
        if (magnitudes[ring] == 0u) continue;
        const int pad = offsets[ring];
        // The colors are blended *before* drawing; no per-pixel alpha work.
        const uint16_t tint = deskpet_mood::blend565(
            background, ring == 2u ? deskpet_mood::kFuriousEye
                                   : deskpet_mood::kFuriousBrow,
            magnitudes[ring]);
        stroke(left - pad, top - pad, width + 2 * pad, height + 2 * pad,
               radius + pad, tint);
        // A neighboring stroke softens the jagged 1px outline on IPS.
        stroke(left - pad + 1, top - pad + 1,
               width + 2 * pad - 2, height + 2 * pad - 2,
               radius + pad - 1, tint);
    }
}

} // namespace deskpet_fury_glow
