#pragma once

#include <stdint.h>
#include <string.h>

#include "Emotion.h"

// DeskPet serial protocol v1. ASCII one-line request/response. No JSON,
// heap, Arduino or network dependencies. Existing human Serial commands
// remain unchanged; dp1 is reserved for the app and is intentionally strict.
namespace deskpet_wire {

enum class Action : uint8_t {
    Invalid,
    Hello,
    Ping,
    Status,
    Emotion,
    Sleep,
    Wake
};

struct Request {
    Action action = Action::Invalid;
    Emotion emotion = Emotion::Neutral;
};

static constexpr uint8_t kVersion = 1u;
static constexpr uint8_t kMaxLineLength = 47u;

inline const char* emotionToken(Emotion emotion) {
    static const char* const kNames[] = {
        "neutral", "happy", "curious", "surprised", "sleepy",
        "angry", "sad", "love", "confused", "excited",
        "playful", "smug", "furious", "embarrassed", "dizzy",
        "bored", "bewildered", "nerd", "derp", "gremlin",
        "panic", "unibrow", "scruffy", "sideeye", "puffed", "goofy"
    };
    const uint8_t index = static_cast<uint8_t>(emotion);
    return index < sizeof(kNames) / sizeof(kNames[0]) ? kNames[index]
                                                      : nullptr;
}

inline bool parse(const char* line, Request& out) {
    out = Request{};
    if (line == nullptr) return false;
    const size_t length = strlen(line);
    if (length > kMaxLineLength || strncmp(line, "dp1 ", 4) != 0)
        return false;
    const char* payload = line + 4;
    if (strcmp(payload, "hello") == 0) out.action = Action::Hello;
    else if (strcmp(payload, "ping") == 0) out.action = Action::Ping;
    else if (strcmp(payload, "status") == 0) out.action = Action::Status;
    else if (strcmp(payload, "sleep") == 0) out.action = Action::Sleep;
    else if (strcmp(payload, "wake") == 0) out.action = Action::Wake;
    else if (strncmp(payload, "emotion ", 8) == 0) {
        const char* name = payload + 8;
        for (uint8_t i = 0u; i < static_cast<uint8_t>(Emotion::Count); ++i) {
            const Emotion candidate = static_cast<Emotion>(i);
            if (strcmp(name, emotionToken(candidate)) == 0) {
                out.action = Action::Emotion;
                out.emotion = candidate;
                break;
            }
        }
    }
    return out.action != Action::Invalid;
}

} // namespace deskpet_wire
