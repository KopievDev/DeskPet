#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ReactionController.h"

static void checkMood(ReactionController& reactions, Emotion expected) {
    Emotion actual = Emotion::Count;
    assert(reactions.takeChange(actual));
    assert(actual == expected);
    assert(!reactions.takeChange(actual)); // do not restart the animation
}

int main() {
    ReactionController reactions;
    reactions.begin(1000);
    assert(reactions.manualEmotion() == Emotion::Neutral);
    assert(!reactions.reactionActive());
    Emotion unused = Emotion::Count;
    assert(!reactions.takeChange(unused));

    // A build failure is transient and does not replace the manual choice.
    reactions.chooseEmotion(Emotion::Happy, 2000);
    checkMood(reactions, Emotion::Happy);
    reactions.buildResult(false, 3000);
    checkMood(reactions, Emotion::Furious);
    assert(reactions.manualEmotion() == Emotion::Happy);
    reactions.tick(3000 + ReactionController::kReactionDurationMs - 1);
    assert(!reactions.takeChange(unused));
    reactions.tick(3000 + ReactionController::kReactionDurationMs);
    checkMood(reactions, Emotion::Happy);

    // Success replaces an active error reaction, but still restores Happy.
    reactions.buildResult(false, 9000);
    checkMood(reactions, Emotion::Furious);
    reactions.buildResult(true, 9200);
    checkMood(reactions, Emotion::Excited);
    reactions.tick(9200 + ReactionController::kReactionDurationMs);
    checkMood(reactions, Emotion::Happy);

    // Manual input preempts a pending build reaction.
    reactions.buildResult(false, 16000);
    checkMood(reactions, Emotion::Furious);
    reactions.chooseEmotion(Emotion::Love, 16050);
    checkMood(reactions, Emotion::Love);
    reactions.tick(21000);
    assert(!reactions.takeChange(unused));

    // Bored happens only after two minutes of inactivity, once.
    reactions.begin(22000);
    reactions.tick(22000 + ReactionController::kDefaultIdleSeconds * 1000 - 1);
    assert(!reactions.takeChange(unused));
    reactions.tick(22000 + ReactionController::kDefaultIdleSeconds * 1000);
    checkMood(reactions, Emotion::Bored);
    assert(reactions.idleActive());
    reactions.tick(999999);
    assert(!reactions.takeChange(unused));
    reactions.activity(1000000);
    checkMood(reactions, Emotion::Love);

    // Idle can be turned off or have a constrained timeout.
    reactions.setIdleEnabled(false, 1200000);
    reactions.tick(1200000 + 600000);
    assert(!reactions.takeChange(unused));
    assert(!reactions.setIdleSeconds(14, 1000));
    assert(!reactions.setIdleSeconds(3601, 1000));
    assert(reactions.setIdleSeconds(15, 2000000));
    reactions.setIdleEnabled(true, 2000000);
    reactions.tick(2014999);
    assert(!reactions.takeChange(unused));
    reactions.tick(2015000);
    checkMood(reactions, Emotion::Bored);
    reactions.chooseEmotion(Emotion::Curious, 2015001);
    checkMood(reactions, Emotion::Curious);

    // Elapsed time works across the millis() wrap-around.
    const uint32_t nearWrap = UINT32_MAX - 500;
    reactions.begin(nearWrap);
    reactions.buildResult(true, nearWrap);
    checkMood(reactions, Emotion::Excited);
    reactions.tick(nearWrap + 4501u);
    checkMood(reactions, Emotion::Curious);

    // USER cycles the manually selected emotion, not the temporary one.
    reactions.begin(4000);
    reactions.chooseEmotion(Emotion::Smug, 4010);
    checkMood(reactions, Emotion::Smug);
    reactions.buildResult(false, 4200);
    checkMood(reactions, Emotion::Furious);
    reactions.nextEmotion(4300);
    assert(reactions.manualEmotion() == Emotion::Furious);
    assert(!reactions.takeChange(unused)); // already showing Furious; no redraw
    assert(!reactions.reactionActive());
    reactions.nextEmotion(4400);
    checkMood(reactions, Emotion::Embarrassed);

    // USER/demo must include all four comic emotions before wrapping.
    reactions.begin(5000);
    reactions.chooseEmotion(Emotion::Bored, 5010);
    checkMood(reactions, Emotion::Bored);
    reactions.nextEmotion(5020);
    checkMood(reactions, Emotion::Bewildered);
    reactions.nextEmotion(5030);
    checkMood(reactions, Emotion::Nerd);
    reactions.nextEmotion(5040);
    checkMood(reactions, Emotion::Derp);
    reactions.nextEmotion(5050);
    checkMood(reactions, Emotion::Gremlin);
    reactions.nextEmotion(5060);
    checkMood(reactions, Emotion::Panic);
    reactions.nextEmotion(5070);
    checkMood(reactions, Emotion::Neutral);

    // USER hold "pet" shows Love briefly, then restores the manual
    // mood, without altering the user's selection.
    reactions.begin(6000u);
    reactions.chooseEmotion(Emotion::Angry, 6050u);
    checkMood(reactions, Emotion::Angry);
    reactions.showTemporary(Emotion::Love, 6100u, 3200u);
    checkMood(reactions, Emotion::Love);
    assert(reactions.manualEmotion() == Emotion::Angry);
    reactions.tick(9299u);
    assert(!reactions.takeChange(unused));
    reactions.tick(9300u);
    checkMood(reactions, Emotion::Angry);

    // USER double-tap shows a transient Surprised; another manual
    // action cancels it immediately.
    reactions.showTemporary(Emotion::Surprised, 9500u, 2300u);
    checkMood(reactions, Emotion::Surprised);
    reactions.chooseEmotion(Emotion::Happy, 9800u);
    checkMood(reactions, Emotion::Happy);
    reactions.tick(12000u);
    assert(!reactions.takeChange(unused));

    // Consecutive temporary reactions replace one another safely.
    reactions.showTemporary(Emotion::Love, 14000u, 3200u);
    checkMood(reactions, Emotion::Love);
    reactions.showTemporary(Emotion::Surprised, 14200u, 2300u);
    checkMood(reactions, Emotion::Surprised);
    reactions.tick(16500u);
    checkMood(reactions, Emotion::Happy);

    // A transient event does not change the manual emotion even if its
    // visual mood already matches the saved baseline.
    reactions.showTemporary(Emotion::Happy, 18000u, 3200u);
    assert(!reactions.takeChange(unused));
    reactions.tick(21200u);
    assert(!reactions.takeChange(unused));

    puts("PASS: build/idle reaction scheduling, manual priority, no repeats, wrap-around");
    return 0;
}
