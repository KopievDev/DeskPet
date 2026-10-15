#include <assert.h>
#include <stdint.h>
#include <initializer_list>
#include <stdio.h>

#include "EmotionParticles.h"

int main() {
    using namespace deskpet_particles;

    assert(styleFor(Emotion::Excited) == Style::Sparks);
    assert(styleFor(Emotion::Love) == Style::Hearts);
    assert(styleFor(Emotion::Furious) == Style::Steam);
    assert(styleFor(Emotion::Happy) == Style::None);
    assert(styleFor(Emotion::Angry) == Style::None);
    assert(styleFor(Emotion::Neutral) == Style::None);
    assert(styleFor(Emotion::Count) == Style::None);
    assert(kParticleCount == 6u);

    // Both left and right gutters are populated, but no particle can
    // enter the eyes, eyebrows, status strip or top comic bubble.
    for (const Style style : {Style::Sparks, Style::Hearts, Style::Steam}) {
        bool seenLeft = false;
        bool seenRight = false;
        bool seenMoving = false;
        for (uint32_t now = 0; now <= 30000; now += 41u) {
            for (uint8_t i = 0; i < kParticleCount; ++i) {
                const Particle p = sample(style, now, i);
                const Particle again = sample(style, now, i);
                assert(p.x == again.x && p.y == again.y);
                assert(p.opacity == again.opacity && p.size == again.size);
                assert(p.x >= kMinX + 8 && p.x <= kMaxX - 8);
                assert(p.y >= kMinY + 7 && p.y <= kMaxY - 7);
                assert(p.opacity <= 255);
                assert(p.size == 2u || p.size == 3u);
                if (i < 3u) {
                    assert(p.x < 61);
                    seenLeft = true;
                } else {
                    assert(p.x > 259);
                    seenRight = true;
                }
                if (p.y < 100) seenMoving = true;
            }
        }
        assert(seenLeft && seenRight && seenMoving);
    }

    // A complete life cycle starts invisible, rises steadily and ends
    // invisible. The offset staggering applies only to indices > 0.
    const Particle start = sample(Style::Sparks, 0u, 0u);
    const Particle quarter = sample(Style::Sparks, 600u, 0u);
    const Particle middle = sample(Style::Sparks, 1200u, 0u);
    const Particle finish = sample(Style::Sparks, 2399u, 0u);
    assert(start.opacity == 0u);
    assert(quarter.opacity == 255u);
    assert(middle.y < quarter.y && quarter.y < start.y);
    assert(finish.opacity <= 1u);
    assert(sample(Style::Sparks, 2400u, 0u).opacity == 0u);

    // No effects for unrelated expressions or out-of-range IDs.
    const Particle absent = sample(Style::None, 999u, 0u);
    assert(absent.opacity == 0u && absent.size == 0u);
    const Particle invalid = sample(Style::Hearts, 999u, kParticleCount);
    assert(invalid.opacity == 0u && invalid.size == 0u);

    puts("PASS: six deterministic particles, bounds, staggered cycles, fades and mood mapping");
    return 0;
}
