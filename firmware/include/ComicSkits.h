#pragma once

#include <stdint.h>

#include "Emotion.h"

// Compact, predictable "acting beats" for the four joke expressions.
// The same function is shared by pupils/accessories, so jokes are timed
// as a coherent little scene rather than unrelated animation loops.
// Everything is local, allocation-free and deterministic across millis().
namespace deskpet_comics {

constexpr uint32_t kSceneDurationMs = 4800u;

struct Frame {
    int8_t leftPupil = 0;
    int8_t rightPupil = 0;
    int8_t glassesLift = 0;
    int8_t tongueSway = 0;
    int8_t tongueDrop = 0;
    int8_t grinLift = 0;
    int8_t jawShake = 0;
    int8_t sweatDrop = 0;
    uint8_t glint = 0;
};

inline bool isComic(Emotion mood) {
    return mood == Emotion::Nerd || mood == Emotion::Derp ||
           mood == Emotion::Gremlin || mood == Emotion::Panic;
}

inline uint32_t lerpEnvelope(uint32_t t, uint32_t riseStart,
                             uint32_t riseEnd, uint32_t fallStart,
                             uint32_t fallEnd, uint32_t magnitude) {
    if (t <= riseStart || t >= fallEnd) return 0u;
    if (t < riseEnd) {
        const uint32_t u = (t - riseStart) * 256u / (riseEnd - riseStart);
        const uint32_t smooth = u * u * (768u - 2u * u) / 65536u;
        return magnitude * smooth / 256u;
    }
    if (t <= fallStart) return magnitude;
    const uint32_t u = (fallEnd - t) * 256u / (fallEnd - fallStart);
    const uint32_t smooth = u * u * (768u - 2u * u) / 65536u;
    return magnitude * smooth / 256u;
}

inline Frame at(Emotion mood, uint32_t elapsedMs) {
    Frame frame;
    const uint32_t t = elapsedMs % kSceneDurationMs;
    switch (mood) {
        case Emotion::Nerd: {
            // "Wait, my glasses!": spectacles slide up and sparkle,
            // then the shy gaze settles back to normal.
            frame.glassesLift = static_cast<int8_t>(lerpEnvelope(
                t, 750u, 1110u, 1700u, 2130u, 6u));
            frame.leftPupil = static_cast<int8_t>(lerpEnvelope(
                t, 1250u, 1570u, 2160u, 2500u, 4u));
            frame.rightPupil = -frame.leftPupil;
            frame.glint = static_cast<uint8_t>(lerpEnvelope(
                t, 1600u, 1830u, 2240u, 2520u, 230u));
            break;
        }
        case Emotion::Derp: {
            // First cross-eyed, then each eye drifts absurdly outward.
            const int move = static_cast<int>(lerpEnvelope(
                t, 800u, 1700u, 2500u, 3600u, 18u));
            frame.leftPupil = static_cast<int8_t>(11 - move);
            frame.rightPupil = static_cast<int8_t>(-10 + move);
            // Tongue's slight delay makes the gag feel deliberate.
            const int tongue = static_cast<int>(lerpEnvelope(
                t, 900u, 1500u, 2900u, 3600u, 4u));
            frame.tongueSway = static_cast<int8_t>(tongue *
                (t < 2450u ? 1 : -1));
            frame.tongueDrop = static_cast<int8_t>(lerpEnvelope(
                t, 500u, 1140u, 2780u, 3580u, 5u));
            break;
        }
        case Emotion::Gremlin: {
            // The grin creeps upward; eyes dart toward the viewer and
            // a tiny gleam makes the moment mischievous.
            const int shift = static_cast<int>(lerpEnvelope(
                t, 500u, 950u, 2490u, 3030u, 6u));
            frame.leftPupil = static_cast<int8_t>(shift);
            frame.rightPupil = static_cast<int8_t>(-shift);
            frame.grinLift = static_cast<int8_t>(lerpEnvelope(
                t, 520u, 1140u, 2560u, 3250u, 5u));
            frame.glint = static_cast<uint8_t>(lerpEnvelope(
                t, 1300u, 1510u, 1730u, 2020u, 240u));
            break;
        }
        case Emotion::Panic: {
            // Teeth chatter, gaze shakes, sweat visibly slips down.
            const bool odd = ((t / 95u) & 1u) != 0u;
            frame.leftPupil = odd ? -3 : 3;
            frame.rightPupil = odd ? 4 : -4;
            frame.jawShake = odd ? -2 : 2;
            frame.sweatDrop = static_cast<int8_t>(
                (t % 1950u) * 11u / 1950u);
            break;
        }
        default: break;
    }
    return frame;
}

} // namespace deskpet_comics
