#include <assert.h>
#include <stdio.h>

#include "ComicStories.h"
#include "ReactionController.h"

// Integration contract: a scripted beat uses the existing temporary reaction
// rather than changing manualEmotion. Beat transitions occur before tick().
static void expect(ReactionController& reaction, Emotion expected) {
    Emotion emitted = Emotion::Count;
    assert(reaction.takeChange(emitted));
    assert(emitted == expected);
}

int main() {
    deskpet_stories::Director story;
    ReactionController reaction;
    story.begin();
    reaction.begin(100u);
    deskpet_stories::Beat beat = {Emotion::Neutral, 0u};

    assert(story.startNext(100u, beat));
    reaction.showTemporary(beat.emotion, 100u, beat.durationMs);
    expect(reaction, Emotion::SideEye);
    assert(reaction.manualEmotion() == Emotion::Neutral);
    assert(reaction.reactionActive());

    // Switch at the deadline before ReactionController::tick() would
    // return the visual expression to Neutral.
    assert(story.tick(1500u, true, beat));
    reaction.showTemporary(beat.emotion, 1500u, beat.durationMs);
    reaction.tick(1500u);
    expect(reaction, Emotion::Unibrow);
    assert(reaction.manualEmotion() == Emotion::Neutral);

    assert(story.tick(3100u, true, beat));
    reaction.showTemporary(beat.emotion, 3100u, beat.durationMs);
    reaction.tick(3100u);
    expect(reaction, Emotion::Goofy);

    assert(!story.tick(4700u, true, beat));
    assert(!story.active());
    reaction.tick(4700u);
    expect(reaction, Emotion::Neutral);
    assert(!reaction.reactionActive());

    // An explicit user emotion cancels a partially playing sketch.
    assert(story.startNext(6000u, beat));
    reaction.showTemporary(beat.emotion, 6000u, beat.durationMs);
    expect(reaction, Emotion::Scruffy);
    story.cancel();
    reaction.chooseEmotion(Emotion::Happy, 6500u);
    expect(reaction, Emotion::Happy);
    assert(!reaction.reactionActive());
    assert(reaction.manualEmotion() == Emotion::Happy);
    assert(!story.tick(12000u, true, beat));
    assert(!reaction.takeChange(beat.emotion));

    // External failure is always more important than the skit.
    reaction.chooseEmotion(Emotion::Neutral, 14000u);
    expect(reaction, Emotion::Neutral);
    assert(story.startNext(15000u, beat));
    reaction.showTemporary(beat.emotion, 15000u, beat.durationMs);
    expect(reaction, Emotion::Surprised);
    story.cancel();
    reaction.buildResult(false, 15050u);
    expect(reaction, Emotion::Furious);
    reaction.tick(19550u);
    expect(reaction, Emotion::Neutral);

    puts("PASS: multi-beat story integration, manual and build priority");
    return 0;
}
