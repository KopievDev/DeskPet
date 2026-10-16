#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "FuryGlow.h"

struct Ring {
    int x;
    int y;
    int w;
    int h;
    int r;
    uint16_t color;
};

struct Capture {
    Ring rings[6] = {};
    int count = 0;

    void add(int x, int y, int w, int h, int r, uint16_t color) {
        assert(count < 6);
        assert(x >= 0 && y >= 0 && w > 0 && h > 0);
        assert(x + w <= 320);
        assert(y + h < 153); // no collision with bottom HUD
        assert(r > 0 && 2 * r <= w && 2 * r <= h);
        assert(color != 0x0862); // a ring must be visible on the background
        rings[count++] = {x, y, w, h, r, color};
    }
};

int main() {
    using namespace deskpet_fury_glow;

    // 1200ms breathing glow, plus a short extra flash during entry.
    const Halo off = alpha(440, 0);
    assert(off.outer == 0 && off.middle == 0 && off.inner == 0);
    const Halo normal = alpha(0, 255);
    const Halo flash = alpha(160, 255);
    const Halo strong = alpha(440, 255);
    assert(normal.outer < normal.middle && normal.middle < normal.inner);
    assert(flash.inner > normal.inner);
    assert(flash.inner > strong.inner); // intro flare is visibly stronger
    assert(alpha(1200, 255).inner == normal.inner); // steady loop
    assert(alpha(2400, 255).inner == normal.inner);
    assert(alpha(440, 128).inner < strong.inner);
    assert(alpha(440, 128).inner > 0);

    // Two adjacent strokes per tier, three fixed-cost tiers per eye.
    Capture full;
    draw(63, 67, 69, 55, 27, 440, 255, 0x0862,
         [&full](int x, int y, int w, int h, int r, uint16_t color) {
             full.add(x, y, w, h, r, color);
         });
    assert(full.count == 6);
    assert(full.rings[0].x == 50);  // outer ring, 13px margin
    assert(full.rings[0].y == 54);
    assert(full.rings[2].x == 54);  // middle ring, 9px margin
    assert(full.rings[4].x == 58);  // inner ring, 5px margin

    Capture secondEye;
    draw(188, 67, 69, 55, 27, 440, 255, 0x0862,
         [&secondEye](int x, int y, int w, int h, int r, uint16_t color) {
             secondEye.add(x, y, w, h, r, color);
         });
    assert(secondEye.count == 6);

    Capture skipped;
    draw(63, 67, 69, 55, 27, 440, 0, 0x0862,
         [&skipped](int x, int y, int w, int h, int r, uint16_t color) {
             skipped.add(x, y, w, h, r, color);
         });
    assert(skipped.count == 0);
    draw(63, 67, 0, 55, 27, 440, 255, 0x0862,
         [&skipped](int x, int y, int w, int h, int r, uint16_t color) {
             skipped.add(x, y, w, h, r, color);
         });
    assert(skipped.count == 0);

    // Strength 0 and blink suppression happen in FaceAnimator::drawEye.
    // The pure renderer produces no extra allocations or full-screen fill.
    puts("PASS: Furious halo pulse, flare, RGB565 rings, bounds and no-op");
    return 0;
}
