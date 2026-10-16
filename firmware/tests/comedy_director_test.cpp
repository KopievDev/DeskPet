#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <initializer_list>

#include "ComedyDirector.h"

using deskpet_comedy::Cue;
using deskpet_comedy::Director;

static bool funny(Emotion mood) {
    return mood == Emotion::Unibrow || mood == Emotion::Scruffy ||
           mood == Emotion::SideEye || mood == Emotion::Puffed ||
           mood == Emotion::Goofy;
}

int main() {
    Director a, b;
    a.begin(0u, 42u);
    b.begin(0u, 42u);
    assert(a.enabled() && a.count() == 0u);
    assert(Director::kSceneDurationMs == 4800u);
    for (const Personality type :
         {Personality::Calm, Personality::Playful, Personality::Cheeky}) {
        assert(Director::minimumInterval(type) <
               Director::maximumInterval(type));
    }
    Cue x = {Emotion::Count, 0u};
    Cue y = {Emotion::Count, 0u};
    Emotion last = Emotion::Count;
    for (uint32_t now = 1000u; now < 2000000u; now += 1000u) {
        const bool first = a.tick(now, true, Personality::Playful,
                                  80u, 85u, 55u, 90u, x);
        const bool second = b.tick(now, true, Personality::Playful,
                                   80u, 85u, 55u, 90u, y);
        assert(first == second);
        if (first) {
            assert(x.emotion == y.emotion);
            assert(x.durationMs == 4800u);
            assert(funny(x.emotion));
            assert(x.emotion != last);
            last = x.emotion;
        }
    }
    assert(a.count() >= 10u);
    assert(a.count() == b.count());
    assert(a.lastEmotion() == b.lastEmotion());

    // Playing higher-priority temporary reactions must pause the clock,
    // not reset accumulated quiet time (or comedy would starve).
    Director paused;
    paused.begin(0u, 9u);
    for (uint32_t now = 1000u; now < 50000u; now += 1000u)
        assert(!paused.tick(now, true, Personality::Calm,
                            50u, 50u, 50u, 50u, x));
    const uint32_t accrued = paused.eligibleMs();
    assert(accrued >= 47000u && accrued <= 49000u);
    for (uint32_t now = 50000u; now < 250000u; now += 1000u)
        assert(!paused.tick(now, false, Personality::Calm,
                            50u, 50u, 50u, 50u, x));
    assert(paused.eligibleMs() == accrued);
    assert(!paused.tick(250000u, true, Personality::Calm,
                        50u, 50u, 50u, 50u, x));
    assert(paused.eligibleMs() == accrued);
    bool emitted = false;
    for (uint32_t now = 251000u; now < 400000u; now += 1000u) {
        if (paused.tick(now, true, Personality::Calm,
                        50u, 50u, 50u, 50u, x)) {
            emitted = true;
            break;
        }
    }
    assert(emitted && paused.count() == 1u);

    paused.setEnabled(false, 400000u);
    assert(!paused.enabled());
    for (uint32_t now = 401000u; now < 700000u; now += 1000u)
        assert(!paused.tick(now, true, Personality::Calm,
                            50u, 50u, 50u, 50u, x));
    paused.setEnabled(true, 700000u);
    assert(paused.remainingMs() >= Director::minimumInterval(
        Personality::Calm));
    assert(!paused.tick(700001u, true, Personality::Calm,
                        50u, 50u, 50u, 50u, x));

    // Manual preview chooses only the five comic faces, skips repeats,
    // and cannot alter the game scores sent to the method.
    Director preview;
    preview.begin(15u, 19u);
    last = Emotion::Count;
    for (uint32_t now = 15u; now < 3015u; now += 3u) {
        assert(preview.preview(now, Personality::Cheeky,
                               12u, 20u, 8u, 95u, x));
        assert(funny(x.emotion) && x.emotion != last);
        assert(x.durationMs == 4800u);
        last = x.emotion;
    }
    assert(preview.count() == 1000u);
    preview.activity(3020u);
    assert(preview.eligibleMs() == 0u);
    assert(preview.count() == 1000u);

    // uint32 millis() wraps: subtraction must remain monotonic for
    // normal frame deltas and never spontaneously emit.
    Director rollover;
    rollover.begin(0xFFFFF000u, 12u);
    assert(!rollover.tick(0xFFFFF100u, true, Personality::Calm,
                          78u, 70u, 65u, 50u, x));
    assert(!rollover.tick(0x00000020u, true, Personality::Calm,
                          78u, 70u, 65u, 50u, x));
    assert(rollover.count() == 0u);
    assert(rollover.eligibleMs() < 10000u);
    puts("PASS: autonomous comedy, priority pause, weights, determinism, wrap");
    return 0;
}
