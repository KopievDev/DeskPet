#pragma once

#include <stdint.h>

// RGB565 colours for the IPS ST7789. The angry palettes apply only to the
// Angry/Furious face; all other emotions use their existing pigments.
// No dynamic allocations or float math in this palette helper.
namespace deskpet_mood {
constexpr uint16_t kNormalEye = 0xDFFF;
constexpr uint16_t kNormalPupil = 0x0882;
constexpr uint16_t kNormalAccent = 0x5F1F;

constexpr uint16_t kAngryEye = 0xFE91;     // #FFD18B - warm amber
constexpr uint16_t kAngryPupil = 0x4122;   // #422617 - deep brown
constexpr uint16_t kAngryBrow = 0xFCC6;    // #FF9A36 - vivid orange

constexpr uint16_t kFuriousEye = 0xF9CA;   // #FF3B55 - deep scarlet, distinct from Love
constexpr uint16_t kFuriousPupil = 0x4044; // #410B20 - deep burgundy
constexpr uint16_t kFuriousBrow = 0xF929;  // #FF244A - crimson
constexpr uint16_t kFuriousSpark = 0xFEAB; // #FFD45C - golden lightning

// Interpolate each RGB565 component independently; blending packed 16-bit
// values directly would change unrelated color channels.
inline uint16_t blend565(uint16_t from, uint16_t to, uint8_t alpha) {
    if (alpha == 0u) return from;
    if (alpha == 255u) return to;
    const uint32_t inv = 255u - alpha;
    const uint32_t r = (((from >> 11) & 31u) * inv +
                        ((to >> 11) & 31u) * alpha + 127u) / 255u;
    const uint32_t g = (((from >> 5) & 63u) * inv +
                        ((to >> 5) & 63u) * alpha + 127u) / 255u;
    const uint32_t b = ((from & 31u) * inv +
                        (to & 31u) * alpha + 127u) / 255u;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

struct Colors {
    uint16_t eye;
    uint16_t pupil;
    uint16_t brow;
};

// The two strengths are animated independently in FaceAnimator. An Angry ->
// Furious transition therefore moves through a gradual orange/red blend,
// not an abrupt swap. Pass the normal brow colour from the caller, so the
// muted Neutral and Sleepy palettes are left untouched.
inline Colors colors(uint8_t angryStrength, uint8_t furiousStrength,
                     uint16_t baseBrow) {
    Colors result = {
        blend565(blend565(kNormalEye, kAngryEye, angryStrength),
                 kFuriousEye, furiousStrength),
        blend565(blend565(kNormalPupil, kAngryPupil, angryStrength),
                 kFuriousPupil, furiousStrength),
        blend565(blend565(baseBrow, kAngryBrow, angryStrength),
                 kFuriousBrow, furiousStrength),
    };
    return result;
}
} // namespace deskpet_mood
