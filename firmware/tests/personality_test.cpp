#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "IdleBehavior.h"
#include "Personality.h"

struct Stats {
    int total = 0;
    int squints = 0;
    int yawns = 0;
    int winks = 0;
};

static Stats simulate(Personality personality, uint32_t seed) {
    IdleBehavior idle;
    idle.begin(2000u, seed);
    idle.setPersonality(personality, 2000u);
    assert(idle.personality() == personality);
    assert(!idle.tick(2000u, true)); // initial arming

    const uint32_t minInterval = IdleBehavior::minInterval(personality);
    const uint32_t maxInterval = IdleBehavior::maxInterval(personality);
    uint32_t lastEnd = 2000u;
    Stats result;
    for (int i = 0; i < 350; ++i) {
        bool started = false;
        for (uint32_t wait = 100u; wait <= maxInterval + 100u; wait += 100u) {
            const uint32_t now = lastEnd + wait;
            if (idle.tick(now, true)) {
                assert(idle.isActive());
                assert(wait >= minInterval && wait <= maxInterval + 100u);
                const IdleGesture gesture = idle.gesture();
                assert(gesture != IdleGesture::None);
                ++result.total;
                if (gesture == IdleGesture::Squint) ++result.squints;
                if (gesture == IdleGesture::Yawn) ++result.yawns;
                if (gesture == IdleGesture::Wink) ++result.winks;
                lastEnd = now + IdleBehavior::duration(gesture);
                assert(idle.tick(lastEnd, true));
                assert(!idle.isActive());
                started = true;
                break;
            }
        }
        assert(started);
    }
    return result;
}

int main() {
    IdleBehavior idle;
    idle.begin(100u, 42u);
    assert(idle.personality() == Personality::Calm);
    assert(personalityName(Personality::Calm)[0] == 'c');
    assert(personalityName(Personality::Playful)[0] == 'p');
    assert(personalityName(Personality::Cheeky)[0] == 'c');
    assert(IdleBehavior::minInterval(Personality::Calm) == 14000u);
    assert(IdleBehavior::maxInterval(Personality::Calm) == 26000u);
    assert(IdleBehavior::minInterval(Personality::Playful) == 6000u);
    assert(IdleBehavior::maxInterval(Personality::Playful) == 12000u);
    assert(IdleBehavior::minInterval(Personality::Cheeky) == 9000u);
    assert(IdleBehavior::maxInterval(Personality::Cheeky) == 16000u);

    const Stats calm = simulate(Personality::Calm, 0xFEED1234u);
    const Stats playful = simulate(Personality::Playful, 0xFEED1234u);
    const Stats cheeky = simulate(Personality::Cheeky, 0xFEED1234u);
    assert(calm.total == 350 && playful.total == 350 && cheeky.total == 350);
    // Statistical sanity checks for highly separated weighting, with a
    // deterministic RNG seed so any regression is reproducible.
    assert(calm.yawns > cheeky.yawns);
    assert(playful.winks > calm.winks);
    assert(cheeky.squints > playful.squints);

    // Switching personality immediately cancels an active gesture and
    // does not switch off the ambient subsystem.
    idle.tick(100u, true);
    bool began = false;
    uint32_t start = 100u;
    for (uint32_t wait = 100u; wait <= 26100u; wait += 100u) {
        if (idle.tick(100u + wait, true)) {
            start = 100u + wait;
            began = true;
            break;
        }
    }
    assert(began && idle.isActive());
    idle.setPersonality(Personality::Cheeky, start + 10u);
    assert(idle.personality() == Personality::Cheeky);
    assert(idle.enabled() && !idle.isActive());
    assert(idle.gesture() == IdleGesture::None);
    assert(!idle.tick(start + 10u, true)); // start a fresh quiet interval

    // Ambient off stays off even if another profile is selected.
    idle.setEnabled(false, start + 20u);
    idle.setPersonality(Personality::Playful, start + 30u);
    assert(!idle.enabled());
    assert(!idle.tick(start + 100000u, true));

    puts("PASS: calm/playful/cheeky cadence, weighted gestures and cancellation");
    return 0;
}
