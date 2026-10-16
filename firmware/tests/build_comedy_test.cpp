#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "BuildComedy.h"

using deskpet_build_comedy::Cue;
using deskpet_build_comedy::Story;

static void expect(const Cue& cue, Emotion emotion, uint32_t duration) {
    assert(cue.emotion == emotion);
    assert(cue.durationMs == duration);
}

int main() {
    Story story;
    story.begin();
    assert(story.consecutiveFailures() == 0u);

    // A normal passing build is exciting, not a strange Nerd appearance.
    expect(story.record(true, 1000u), Emotion::Excited, 4500u);
    assert(story.consecutiveFailures() == 0u);

    // The first mistake is a visual gag, escalation is deliberately
    // comedic, and after 3+ failures it alternates instead of freezing
    // on the same face (new event => new skit animation).
    expect(story.record(false, 2000u), Emotion::Panic, 4800u);
    assert(story.consecutiveFailures() == 1u);
    expect(story.record(false, 3000u), Emotion::Gremlin, 4800u);
    assert(story.consecutiveFailures() == 2u);
    expect(story.record(false, 4000u), Emotion::Furious, 4500u);
    expect(story.record(false, 5000u), Emotion::Gremlin, 4800u);
    expect(story.record(false, 6000u), Emotion::Furious, 4500u);
    assert(story.consecutiveFailures() == 5u);

    // Fixing a broken project makes the pet smug in its nerd glasses.
    expect(story.record(true, 7000u), Emotion::Nerd, 4800u);
    assert(story.consecutiveFailures() == 0u);
    expect(story.record(true, 8000u), Emotion::Excited, 4500u);

    // Stale failures are not carried across unrelated builds.
    story.reset();
    expect(story.record(false, 10000u), Emotion::Panic, 4800u);
    expect(story.record(false, 610001u), Emotion::Panic, 4800u);
    assert(story.consecutiveFailures() == 1u);
    expect(story.record(true, 1210002u), Emotion::Excited, 4500u);
    assert(story.consecutiveFailures() == 0u);

    // Inclusive edge of the 10-minute window remains one streak.
    story.reset();
    expect(story.record(false, 10000u), Emotion::Panic, 4800u);
    expect(story.record(false, 610000u), Emotion::Gremlin, 4800u);
    expect(story.record(true, 1210000u), Emotion::Nerd, 4800u);

    // Saturation cannot wrap to 0 or restart Panic while there is
    // still an active series.
    story.reset();
    for (uint32_t i = 0; i < 200u; ++i) {
        const Cue cue = story.record(false, 2000000u + i * 250u);
        if (i == 0u) expect(cue, Emotion::Panic, 4800u);
        else if (i % 2u != 0u) expect(cue, Emotion::Gremlin, 4800u);
        else expect(cue, Emotion::Furious, 4500u);
        assert(story.consecutiveFailures() <= 99u);
    }
    assert(story.consecutiveFailures() == 99u);
    story.reset();
    assert(story.consecutiveFailures() == 0u);

    // unsigned millis() arithmetic survives wrap-around.
    const uint32_t edge = UINT32_MAX - 200u;
    expect(story.record(false, edge), Emotion::Panic, 4800u);
    expect(story.record(false, edge + 1000u), Emotion::Gremlin, 4800u);
    expect(story.record(true, edge + 1100u), Emotion::Nerd, 4800u);
    expect(story.record(true, edge + 1200u), Emotion::Excited, 4500u);

    puts("PASS: build comedy escalation, Nerd recovery, manual-safe cues and rollover");
    return 0;
}
