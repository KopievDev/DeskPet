#pragma once

#include <stdint.h>

// Personality controls ambient gesture cadence/weighting and the language
// of optional comic bubbles. It never overrides explicit user emotions.
enum class Personality : uint8_t {
    Calm = 0,
    Playful,
    Cheeky
};

inline const char* personalityName(Personality value) {
    switch (value) {
        case Personality::Calm: return "calm";
        case Personality::Playful: return "playful";
        case Personality::Cheeky: return "cheeky";
    }
    return "calm";
}
