#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "UserActivity.h"

using deskpet_activity::Controller;
using deskpet_activity::Cue;
using deskpet_activity::Presence;

int main() {
    Controller activity;
    activity.begin(1000u);
    Cue cue = {Emotion::Neutral, 0u};
    assert(activity.enabled());
    assert(activity.presence() == Presence::Unknown);
    assert(!activity.tick(1000u, true, Personality::Calm, 50, cue));

    // First Mac signal establishes initial state without interrupting
    // the face; repeated heartbeat-like reports do not trigger anything.
    activity.active(1100u);
    assert(activity.presence() == Presence::Active);
    assert(!activity.tick(1101u, true, Personality::Calm, 50, cue));
    activity.active(1200u);
    assert(!activity.tick(1201u, true, Personality::Calm, 50, cue));

    activity.idle(2000u);
    assert(activity.presence() == Presence::Away);
    assert(activity.tick(2001u, true, Personality::Calm, 50, cue));
    assert(cue.emotion == Emotion::Bored);
    assert(cue.durationMs == Controller::kShortDurationMs);
    assert(!activity.tick(2002u, true, Personality::Calm, 50, cue));
    activity.idle(2200u);
    assert(!activity.tick(2200u, true, Personality::Calm, 50, cue));

    // Jokes are rare and are gated by manual/Demo/sleep/reaction state.
    assert(!activity.tick(92000u, false, Personality::Calm, 50, cue));
    assert(activity.tick(92001u, true, Personality::Calm, 50, cue));
    assert(cue.emotion == Emotion::Nerd);
    assert(activity.jokeCount() == 1u);
    assert(!activity.tick(92002u, true, Personality::Calm, 50, cue));
    assert(activity.tick(182001u, true, Personality::Calm, 50, cue));
    assert(cue.emotion == Emotion::Bored);

    // Return from a long absence: the affection score determines whether
    // the pet loves, teases or celebrates. A scene can be briefly deferred
    // if a build reaction is currently active, but not indefinitely.
    activity.active(190000u);
    assert(activity.presence() == Presence::Active);
    assert(!activity.tick(190010u, false, Personality::Calm, 89, cue));
    assert(activity.tick(192000u, true, Personality::Calm, 89, cue));
    assert(cue.emotion == Emotion::Love);
    assert(!activity.tick(192100u, true, Personality::Calm, 89, cue));

    // First idle event after reconnecting while already away is
    // intentionally silent (no mysterious spontaneous Bored).
    activity.begin(0u);
    activity.idle(100u);
    assert(!activity.tick(101u, true, Personality::Playful, 50, cue));
    assert(activity.tick(90100u, true, Personality::Playful, 50, cue));
    assert(cue.emotion == Emotion::Derp);

    // Short visits are not rewarded: this must not play a "welcome back"
    // reaction for an accidental screen wake.
    activity.begin(0u);
    activity.active(100u);
    activity.idle(200u);
    assert(activity.tick(200u, true, Personality::Calm, 50, cue));
    activity.active(1200u);
    assert(!activity.tick(1201u, true, Personality::Calm, 50, cue));

    // Friendly/cheeky return branches.
    activity.idle(2000u);
    assert(activity.tick(2000u, true, Personality::Calm, 50, cue));
    activity.active(22000u);
    assert(activity.tick(22001u, true, Personality::Cheeky, 40, cue));
    assert(cue.emotion == Emotion::Derp);
    activity.idle(23000u);
    assert(activity.tick(23001u, true, Personality::Calm, 40, cue));
    activity.active(53000u);
    assert(activity.tick(53001u, true, Personality::Calm, 40, cue));
    assert(cue.emotion == Emotion::Excited);

    // A pending return must expire if the user was busy with a build
    // or a manually selected expression for more than 12 seconds.
    activity.idle(54000u);
    assert(activity.tick(54001u, true, Personality::Calm, 50, cue));
    activity.active(70000u);
    assert(!activity.tick(82001u, false, Personality::Calm, 50, cue));
    assert(!activity.tick(82002u, true, Personality::Calm, 50, cue));

    // A disabled helper doesn't create reactions, nor retain stale state.
    activity.setEnabled(false, 83000u);
    assert(!activity.enabled());
    assert(activity.presence() == Presence::Unknown);
    activity.idle(84000u);
    activity.active(85000u);
    assert(!activity.tick(180000u, true, Personality::Cheeky, 90, cue));
    activity.setEnabled(true, 180100u);
    assert(activity.presence() == Presence::Unknown);

    // UINT32 millis rollover, without signed timestamps.
    const uint32_t edge = UINT32_MAX - 5000u;
    activity.begin(edge);
    activity.active(edge + 100u);
    activity.idle(edge + 200u);
    assert(activity.tick(edge + 201u, true, Personality::Calm, 50, cue));
    assert(!activity.tick(edge + 89999u, true, Personality::Calm, 50, cue));
    assert(activity.tick(edge + 90200u, true, Personality::Calm, 50, cue));
    assert(cue.emotion == Emotion::Nerd);

    puts("PASS: privacy-safe Mac activity events, idle/return comedy, priority, rollover");
    return 0;
}
