#pragma once

#include <stdint.h>

#include "Emotion.h"

// Deterministic, allocation-free particle layout for the IPS 320x170 face.
// Rendering is kept in FaceAnimator; this pure module is host-testable.
// Particles live in the narrow outer gutters beside the eyes, not over the
// pupils, eyebrows, lower HUD (y >= 153), or top comic speech bubble.
namespace deskpet_particles {

enum class Style : uint8_t {
    None = 0,
    Sparks,
    Hearts,
    Steam
};

constexpr uint8_t kParticleCount = 6;
constexpr uint32_t kCycleMs = 2400u;
constexpr int kMinX = 24;
constexpr int kMaxX = 296;
constexpr int kMinY = 54;
constexpr int kMaxY = 130;

struct Particle {
    int16_t x;
    int16_t y;
    uint8_t opacity; // 0..255, blended with the fixed screen background
    uint8_t size;    // 2..3 pixels
};

inline Style styleFor(Emotion emotion) {
    switch (emotion) {
        case Emotion::Excited: return Style::Sparks;
        case Emotion::Love: return Style::Hearts;
        case Emotion::Furious: return Style::Steam;
        default: return Style::None;
    }
}

// Stable seed for each particle and lifetime; no global random() calls
// that would perturb blinking, gaze or other behaviors.
inline uint32_t scramble(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

inline Particle sample(Style style, uint32_t elapsedMs, uint8_t index) {
    if (style == Style::None || index >= kParticleCount) return {0, 0, 0, 0};

    // Three particles per side, with staggered life cycles.
    const uint32_t offset = (index % 3u) * (kCycleMs / 3u)
                          + (index / 3u) * (kCycleMs / 6u);
    const uint32_t shifted = elapsedMs + offset;
    const uint32_t phase = shifted % kCycleMs;
    const uint32_t lifetime = shifted / kCycleMs;

    const uint32_t seed = scramble(lifetime * 17u + index * 127u
                                   + static_cast<uint32_t>(style) * 41u);
    const int side = index < 3u ? -1 : 1;
    const int baseX = side < 0 ? 47 : 273;
    const int jitter = static_cast<int>(seed % 11u) - 5;
    const int drift = static_cast<int>((phase * 6u) / kCycleMs);

    // Rising motion: born beside lower outer corners, lift into the upper
    // gutters; always keep all pixels within the visible LCD and away
    // from the status strip.
    const int x = baseX + jitter + side * drift;
    const int y = 123 - static_cast<int>((phase * 56u) / kCycleMs);

    // Ease transparency at the edges of a life cycle so the wrapping
    // particle doesn't pop in or out (no hardware alpha required).
    uint32_t alpha = 255u;
    if (phase < 300u) alpha = (phase * 255u) / 300u;
    else if (phase > kCycleMs - 400u)
        alpha = ((kCycleMs - phase) * 255u) / 400u;
    const uint8_t size = static_cast<uint8_t>(2u + ((seed >> 8) & 1u));
    return {
        static_cast<int16_t>(x),
        static_cast<int16_t>(y),
        static_cast<uint8_t>(alpha),
        size
    };
}

} // namespace deskpet_particles
