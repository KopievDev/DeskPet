#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "DeviceProtocol.h"

using namespace deskpet_wire;

int main() {
    Request req;
    assert(kVersion == 1u);
    assert(kMaxLineLength == 47u);
    assert(!parse(nullptr, req));
    assert(!parse("", req));
    assert(!parse("neutral", req)); // legacy command is NOT a protocol frame
    assert(!parse("dp2 hello", req)); // unsupported version
    assert(!parse("dp1", req));
    assert(!parse("dp1 ", req));
    assert(!parse("dp1 status extra", req));
    assert(!parse("dp1 emotion", req));
    assert(!parse("dp1 emotion count", req));
    assert(!parse("dp1 emotion neutral\n", req));
    assert(!parse("dp1 emotion HAPPY", req));
    assert(!parse("dp1 emotion happy now", req));
    assert(!parse("dp1  emotion happy", req));
    assert(!parse("dp1 emotion aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", req));
    assert(req.action == Action::Invalid);
    assert(parse("dp1 hello", req) && req.action == Action::Hello);
    assert(parse("dp1 ping", req) && req.action == Action::Ping);
    assert(parse("dp1 status", req) && req.action == Action::Status);
    assert(parse("dp1 sleep", req) && req.action == Action::Sleep);
    assert(parse("dp1 wake", req) && req.action == Action::Wake);
    for (uint8_t i = 0u; i < static_cast<uint8_t>(Emotion::Count); ++i) {
        const auto mood = static_cast<Emotion>(i);
        const char* token = emotionToken(mood);
        assert(token && strlen(token) > 0u);
        char frame[48] = {};
        const int len = snprintf(frame, sizeof(frame), "dp1 emotion %s", token);
        assert(len > 0 && len <= kMaxLineLength);
        assert(parse(frame, req) && req.action == Action::Emotion &&
               req.emotion == mood);
    }
    assert(emotionToken(Emotion::Count) == nullptr);
    puts("PASS: DP1 strict parse, all 26 emotions, legacy isolation");
}
