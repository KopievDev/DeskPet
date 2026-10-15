#include <assert.h>
#include <stdint.h>
#include <initializer_list>
#include <stdio.h>

#include "ComicSkits.h"

using deskpet_comics::Frame;

static bool same(const Frame& a, const Frame& b) {
    return a.leftPupil == b.leftPupil &&
           a.rightPupil == b.rightPupil &&
           a.glassesLift == b.glassesLift &&
           a.tongueSway == b.tongueSway &&
           a.tongueDrop == b.tongueDrop &&
           a.grinLift == b.grinLift &&
           a.jawShake == b.jawShake &&
           a.sweatDrop == b.sweatDrop &&
           a.glint == b.glint;
}

int main() {
    using namespace deskpet_comics;
    static_assert(kSceneDurationMs == 4800u, "Comic scene period changed");
    for (Emotion e : {Emotion::Nerd, Emotion::Derp,
                      Emotion::Gremlin, Emotion::Panic}) {
        assert(isComic(e));
        // Periodic source: no random(), floats, heap or side effects.
        for (uint32_t now = 0u; now <= kSceneDurationMs; now += 37u) {
            const Frame a = at(e, now);
            const Frame b = at(e, now + kSceneDurationMs);
            assert(same(a, b));
            assert(a.leftPupil >= -12 && a.leftPupil <= 12);
            assert(a.rightPupil >= -12 && a.rightPupil <= 12);
            assert(a.glassesLift >= 0 && a.glassesLift <= 6);
            assert(a.tongueSway >= -4 && a.tongueSway <= 4);
            assert(a.tongueDrop >= 0 && a.tongueDrop <= 5);
            assert(a.grinLift >= 0 && a.grinLift <= 5);
            assert(a.jawShake >= -2 && a.jawShake <= 2);
            assert(a.sweatDrop >= 0 && a.sweatDrop <= 11);
        }
    }
    assert(!isComic(Emotion::Neutral));
    assert(!isComic(Emotion::Love));
    assert(same(at(Emotion::Neutral, 2000u), Frame{}));

    // Nerd spends time deliberately adjusting glasses; the glint occurs
    // only after the movement has started, then everything relaxes.
    assert(at(Emotion::Nerd, 0u).glassesLift == 0);
    assert(at(Emotion::Nerd, 1400u).glassesLift >= 5);
    assert(at(Emotion::Nerd, 2010u).glint > 50u);
    assert(at(Emotion::Nerd, 2800u).glassesLift == 0);
    assert(at(Emotion::Nerd, 2800u).glint == 0);

    // Derp smoothly drifts pupils apart, with delayed tongue extension.
    assert(at(Emotion::Derp, 0u).leftPupil == 11);
    assert(at(Emotion::Derp, 0u).rightPupil == -10);
    assert(at(Emotion::Derp, 1800u).leftPupil <= -6);
    assert(at(Emotion::Derp, 1800u).rightPupil >= 7);
    assert(at(Emotion::Derp, 1800u).tongueDrop >= 4);
    assert(at(Emotion::Derp, 4100u).leftPupil == 11);
    assert(at(Emotion::Derp, 4800u).rightPupil == -10);

    // Gremlin's grin grows and a tiny gleam punctuates the joke.
    assert(at(Emotion::Gremlin, 0u).grinLift == 0);
    assert(at(Emotion::Gremlin, 1500u).grinLift == 5);
    assert(at(Emotion::Gremlin, 1600u).glint > 80u);
    assert(at(Emotion::Gremlin, 3900u).grinLift == 0);

    // Panic has the frantic rhythm; it resets at the scene boundary.
    const Frame nervous = at(Emotion::Panic, 100u);
    assert(nervous.jawShake != 0);
    assert(at(Emotion::Panic, 1949u).sweatDrop >= 10);
    assert(at(Emotion::Panic, 1950u).sweatDrop == 0);

    // Unsigned arithmetic is safe for elapsed time across millis() rollover.
    const uint32_t nearWrap = UINT32_MAX - 1000u;
    assert(same(at(Emotion::Nerd, nearWrap + 2000u - nearWrap),
                at(Emotion::Nerd, 2000u)));

    puts("PASS: four timed comedy scenes, repeated cycles and bounded offsets");
    return 0;
}
