#pragma once

#include <stdint.h>

// Shared by the ESP32 renderer and the host-testable reaction controller.
// Keep Count last: USER cycles through all selectable expressions.
enum class Emotion : uint8_t {
    Neutral = 0,
    Happy,
    Curious,
    Surprised,
    Sleepy,
    Angry,
    Sad,
    Love,
    Confused,
    Excited,
    Playful,
    Smug,
    Furious,
    Embarrassed,
    Dizzy,
    Bored,
    Count
};
