#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "PetNeeds.h"

using deskpet_needs::Controller;
using deskpet_needs::Cue;
using deskpet_needs::Kind;

static void expect(const Cue& cue, Emotion emotion, Kind kind) {
    assert(cue.emotion == emotion);
    assert(cue.kind == kind);
    assert(cue.durationMs == Controller::kCueDurationMs);
}

int main() {
    Controller needs;
    needs.begin(0u);
    assert(needs.energy() == 78u);
    assert(needs.mood() == 70u);
    assert(needs.attention() == 65u);
    assert(needs.enabled());

    // Awake life is slow. Decay must use elapsed real time, not FPS.
    needs.advance(239999u, false);
    assert(needs.energy() == 78u);
    assert(needs.attention() == 65u);
    needs.advance(240000u, false);
    assert(needs.energy() == 77u);
    assert(needs.attention() == 64u);
    needs.advance(540000u, false);
    assert(needs.energy() == 76u);
    assert(needs.attention() == 63u);
    assert(needs.mood() == 69u);

    needs.pet();
    assert(needs.mood() == 81u && needs.attention() == 83u);
    needs.interact();
    assert(needs.attention() == 86u);
    needs.surprise();
    assert(needs.mood() == 85u && needs.attention() == 94u);
    needs.play();
    assert(needs.energy() == 73u);
    assert(needs.mood() == 95u);
    assert(needs.attention() == 100u);
    needs.coffee();
    assert(needs.energy() == 98u && needs.mood() == 100u);
    needs.build(false);
    assert(needs.energy() == 97u && needs.mood() == 94u);
    needs.build(true);
    assert(needs.mood() == 99u);

    needs.set(8u, 11u, 13u, 600000u);
    Cue cue = {Emotion::Neutral, Kind::None, 0u};
    assert(!needs.tick(600000u, true, Personality::Calm, cue));
    assert(!needs.tick(689999u, true, Personality::Calm, cue));
    assert(needs.tick(690000u, true, Personality::Calm, cue));
    expect(cue, Emotion::Nerd, Kind::Coffee);

    // A busy period (manual emotion, demo, build or sleep) resets the
    // optional cue timer rather than interrupting a stronger action.
    assert(!needs.tick(690001u, false, Personality::Calm, cue));
    assert(!needs.tick(690002u, true, Personality::Calm, cue));

    needs.set(90u, 80u, 12u, 700000u);
    assert(!needs.tick(700000u, true, Personality::Cheeky, cue));
    assert(needs.tick(790000u, true, Personality::Cheeky, cue));
    expect(cue, Emotion::Gremlin, Kind::Attention);

    needs.set(90u, 14u, 90u, 800000u);
    assert(!needs.tick(800000u, true, Personality::Playful, cue));
    assert(needs.tick(890000u, true, Personality::Playful, cue));
    expect(cue, Emotion::Bewildered, Kind::Cheer);

    needs.set(90u, 80u, 90u, 900000u);
    assert(!needs.tick(900000u, true, Personality::Calm, cue));
    assert(!needs.tick(990000u, true, Personality::Calm, cue));
    assert(!needs.tick(1080000u, true, Personality::Calm, cue));

    // Sleeping restores energy without draining attention or mood.
    // Switching energy intervals must not count the previous mode's
    // partial progress (a subtle 4x charging bug at sleep transition).
    needs.set(20u, 50u, 50u, 1200000u);
    needs.advance(1439999u, false);
    assert(needs.energy() == 20u);
    needs.advance(1440000u, true);
    assert(needs.energy() == 20u);
    needs.advance(1500000u, true);
    assert(needs.energy() == 21u);
    assert(needs.attention() == 50u && needs.mood() == 50u);
    needs.advance(1500001u, false);
    assert(needs.energy() == 21u);
    needs.advance(1739999u, false);
    assert(needs.energy() == 21u);
    needs.advance(1740000u, false);
    assert(needs.energy() == 20u);

    // Disabled needs freeze, yet re-enable without catching up from sleep.
    needs.setEnabled(false, 1800000u);
    const uint8_t frozen = needs.energy();
    needs.advance(3000000u, false);
    assert(needs.energy() == frozen);
    assert(!needs.tick(3000000u, true, Personality::Calm, cue));
    needs.setEnabled(true, 3000000u);
    needs.advance(3000001u, false);
    assert(needs.energy() == frozen);

    // Saturation at both ends; manual testing input is clamped.
    needs.set(255u, 255u, 255u, 3100000u);
    assert(needs.energy() == 100u);
    assert(needs.mood() == 100u && needs.attention() == 100u);
    for (int i = 0; i < 30; ++i) needs.coffee();
    assert(needs.energy() == 100u && needs.mood() == 100u);
    needs.set(0u, 0u, 0u, 3200000u);
    needs.build(false);
    assert(needs.energy() == 0u && needs.mood() == 0u);

    // Unsigned millis rollover with normal frame-sized increments.
    const uint32_t edge = UINT32_MAX - 1500u;
    needs.set(30u, 40u, 50u, edge);
    needs.advance(edge + 240000u, false);
    assert(needs.energy() == 29u);
    assert(needs.attention() == 49u);
    needs.advance(edge + 540000u, false);
    assert(needs.mood() == 39u);

    puts("PASS: energy, mood, attention, care, sleep recovery, cues and rollover");
    return 0;
}
