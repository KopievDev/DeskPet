#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ComicExpressions.h"

using namespace deskpet_comic_faces;

int main() {
    assert(kSceneDurationMs == 4800u);
    assert(triangle(0u, 800u, 6u) == 0u);
    assert(triangle(800u, 800u, 6u) == 6u);
    assert(triangle(1600u, 800u, 6u) == 0u);

    const Emotion moods[] = {
        Emotion::Unibrow, Emotion::Scruffy, Emotion::SideEye,
        Emotion::Puffed, Emotion::Goofy
    };
    for (const Emotion mood : moods) {
        const Frame start = at(mood, 0u);
        const Frame repeat = at(mood, kSceneDurationMs);
        assert(start.brow == repeat.brow);
        assert(start.tuft == repeat.tuft);
        assert(start.gaze == repeat.gaze);
        assert(start.cheeks == repeat.cheeks);
        assert(start.jaw == repeat.jaw);
        bool moving = false;
        for (uint32_t elapsed = 0u; elapsed < kSceneDurationMs; elapsed += 37u) {
            const Frame frame = at(mood, elapsed);
            assert(frame.brow >= 0 && frame.brow <= 6);
            assert(frame.tuft >= 0 && frame.tuft <= 7);
            assert(frame.gaze >= -18 && frame.gaze <= 5);
            assert(frame.cheeks >= 0 && frame.cheeks <= 6);
            assert(frame.jaw >= 0 && frame.jaw <= 7);
            assert(frame.brow == at(mood, elapsed + kSceneDurationMs).brow);
            if (frame.brow != start.brow || frame.tuft != start.tuft ||
                frame.gaze != start.gaze || frame.cheeks != start.cheeks ||
                frame.jaw != start.jaw) moving = true;
        }
        assert(moving);
    }
    const Frame neutral = at(Emotion::Neutral, 1500u);
    assert(neutral.brow == 0 && neutral.tuft == 0 &&
           neutral.gaze == 0 && neutral.cheeks == 0 && neutral.jaw == 0);
    puts("PASS: five comic faces, bounds and repeatable animations");
    return 0;
}
