#pragma once

#include <stdint.h>
#include "Emotion.h"

// Small deterministic comedy beats shared by tests and the 320x170 renderer.
// No heap, RNG, Arduino, timers, or floating point. Each act loops in 4.8 s.
namespace deskpet_comic_faces {

static constexpr uint32_t kSceneDurationMs = 4800u;

struct Frame {
    int8_t brow;     // unibrow wiggle
    int8_t tuft;     // scruffy hair movement
    int8_t gaze;     // sideways comic stare
    int8_t cheeks;   // cheek puff
    int8_t jaw;      // springy cartoon mouth
};

inline uint8_t triangle(uint32_t time, uint32_t halfPeriod,
                        uint8_t amplitude) {
    const uint32_t period = halfPeriod * 2u;
    const uint32_t step = time % period;
    const uint32_t rise = step < halfPeriod ? step : period - step;
    return static_cast<uint8_t>(rise * amplitude / halfPeriod);
}

inline Frame at(Emotion emotion, uint32_t elapsedMs) {
    const uint32_t t = elapsedMs % kSceneDurationMs;
    Frame frame = {0, 0, 0, 0, 0};
    switch (emotion) {
        case Emotion::Unibrow:
            frame.brow = static_cast<int8_t>(triangle(t, 800u, 6u));
            frame.jaw = static_cast<int8_t>(triangle(t + 180u, 900u, 3u));
            break;
        case Emotion::Scruffy:
            frame.tuft = static_cast<int8_t>(triangle(t, 400u, 7u));
            frame.jaw = static_cast<int8_t>(triangle(t + 160u, 600u, 5u));
            break;
        case Emotion::SideEye:
            frame.gaze = static_cast<int8_t>(-12 -
                static_cast<int>(triangle(t, 1200u, 6u)));
            frame.brow = static_cast<int8_t>(triangle(t, 1200u, 3u));
            break;
        case Emotion::Puffed:
            frame.cheeks = static_cast<int8_t>(triangle(t, 1200u, 6u));
            frame.jaw = static_cast<int8_t>(triangle(t, 1200u, 3u));
            break;
        case Emotion::Goofy:
            frame.jaw = static_cast<int8_t>(triangle(t, 600u, 7u));
            frame.gaze = static_cast<int8_t>(triangle(t + 240u, 1200u, 5u));
            break;
        default:
            break;
    }
    return frame;
}

} // namespace deskpet_comic_faces
