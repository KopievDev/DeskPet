#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ComicStories.h"

using namespace deskpet_stories;

int main() {
    assert(beatCount(Kind::Suspicious) == 3u);
    assert(beatCount(Kind::Bedhead) == 3u);
    assert(beatCount(Kind::Meltdown) == 4u);
    assert(beatCount(Kind::Count) == 0u);
    assert(beatAt(Kind::Count, 0u).durationMs == 0u);
    assert(beatAt(Kind::Suspicious, 99u).durationMs == 0u);
    assert(beatAt(Kind::Suspicious, 0u).emotion == Emotion::SideEye);
    assert(beatAt(Kind::Suspicious, 1u).emotion == Emotion::Unibrow);
    assert(beatAt(Kind::Suspicious, 2u).emotion == Emotion::Goofy);
    assert(beatAt(Kind::Bedhead, 0u).emotion == Emotion::Scruffy);
    assert(beatAt(Kind::Bedhead, 2u).emotion == Emotion::Derp);
    assert(beatAt(Kind::Meltdown, 0u).emotion == Emotion::Surprised);
    assert(beatAt(Kind::Meltdown, 3u).emotion == Emotion::Happy);
    assert(storyName(Kind::Suspicious)[0] == 's');

    Director director;
    director.begin();
    assert(!director.active() && director.autoEnabled());
    Beat output = {Emotion::Count, 0u};
    assert(director.startNext(100u, output));
    assert(director.active() && director.current() == Kind::Suspicious);
    assert(director.played() == 1u && output.emotion == Emotion::SideEye);
    assert(output.durationMs == 1400u);
    assert(!director.startNext(200u, output)); // no overlapping scripts
    assert(!director.tick(1499u, true, output));
    assert(director.tick(1500u, true, output));
    assert(output.emotion == Emotion::Unibrow && output.durationMs == 1600u);
    assert(director.stage() == 1u);
    assert(!director.tick(3099u, true, output));
    assert(director.tick(3100u, true, output));
    assert(output.emotion == Emotion::Goofy);
    assert(!director.tick(4699u, true, output));
    assert(!director.tick(4700u, true, output));
    assert(!director.active() && director.current() == Kind::Count);

    assert(director.startNext(5000u, output));
    assert(director.current() == Kind::Bedhead);
    assert(!director.tick(6000u, false, output));
    assert(!director.active()); // external event interrupts
    assert(director.startNext(6100u, output));
    assert(director.current() == Kind::Meltdown);
    director.cancel();
    assert(!director.active());
    assert(director.startNext(6200u, output));
    assert(director.current() == Kind::Suspicious); // rotation wraps

    director.setAutoEnabled(false);
    assert(!director.autoEnabled());
    director.cancel();
    assert(director.startNext(7000u, output)); // previews still possible
    assert(!director.tick(17001u, true, output)); // reject stale timestamps
    assert(!director.active());

    Director wrap;
    wrap.begin();
    assert(wrap.startNext(0xFFFFFF00u, output));
    assert(!wrap.tick(0x00000477u, true, output));
    assert(wrap.tick(0x00000478u, true, output));
    assert(output.emotion == Emotion::Unibrow);
    assert(!wrap.tick(0x00000478u, true, output));

    puts("PASS: 3 multi-beat stories, interruption, order and rollover");
    return 0;
}
