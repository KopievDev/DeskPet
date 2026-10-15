#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "MoodPalette.h"

// Verify the exact RGB565 endpoints and that intermediate blending
// doesn't bleed bits from one channel into another.
int main() {
    using namespace deskpet_mood;

    assert(blend565(0xFFFF, 0x0000, 0u) == 0xFFFF);
    assert(blend565(0xFFFF, 0x0000, 255u) == 0x0000);
    assert(blend565(0x0000, 0xF800, 128u) == 0x8000);
    assert(blend565(0x0000, 0x07E0, 128u) == 0x0400);
    assert(blend565(0x0000, 0x001F, 128u) == 0x0010);

    const Colors neutral = colors(0, 0, kNormalAccent);
    assert(neutral.eye == kNormalEye);
    assert(neutral.pupil == kNormalPupil);
    assert(neutral.brow == kNormalAccent);

    const Colors angry = colors(255, 0, kNormalAccent);
    assert(angry.eye == kAngryEye);
    assert(angry.pupil == kAngryPupil);
    assert(angry.brow == kAngryBrow);

    const Colors furious = colors(0, 255, kNormalAccent);
    assert(furious.eye == kFuriousEye);
    assert(furious.pupil == kFuriousPupil);
    assert(furious.brow == kFuriousBrow);

    // All RGB565 transitions must keep red/green/blue channel magnitudes
    // between their endpoints (zero wrap-around or channel contamination).
    for (int alpha = 0; alpha <= 255; ++alpha) {
        const uint16_t value = blend565(kNormalEye, kAngryEye,
                                        static_cast<uint8_t>(alpha));
        for (int shift = 0; shift <= 11; shift += shift == 0 ? 5 : 6) {
            const int mask = shift == 5 ? 63 : 31;
            const int start = (kNormalEye >> shift) & mask;
            const int finish = (kAngryEye >> shift) & mask;
            const int component = (value >> shift) & mask;
            assert(component >= (start < finish ? start : finish));
            assert(component <= (start > finish ? start : finish));
        }
    }

    // Mixing out a mood should restore the original neutral palette.
    const Colors half = colors(128, 0, kNormalAccent);
    assert(half.eye != kNormalEye && half.eye != kAngryEye);
    assert(half.brow != kNormalAccent && half.brow != kAngryBrow);
    const Colors calmAgain = colors(0, 0, 0x94B2);
    assert(calmAgain.eye == kNormalEye);
    assert(calmAgain.pupil == kNormalPupil);
    assert(calmAgain.brow == 0x94B2); // muted neutral/sleep brow preserved

    puts("PASS: Angry/Furious RGB565 endpoints, channel-safe fades and neutral restoration");
    return 0;
}
