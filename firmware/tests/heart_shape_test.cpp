#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "HeartShape.h"

// A tiny stand-in for the sprite: verifies the exact spans the production
// draw routine requests, without LovyanGFX or an ESP32 board.
struct Canvas {
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 128;
    uint8_t pixels[kHeight][kWidth];

    Canvas() { memset(pixels, 0, sizeof(pixels)); }

    void rectangle(int x, int y, int w, int h) {
        assert(w > 0 && h > 0);
        assert(x >= 0 && y >= 0);
        assert(x + w <= kWidth && y + h <= kHeight);
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                pixels[yy][xx] = 1;
    }

    bool at(int x, int y) const { return pixels[y][x] != 0; }

    int filled() const {
        int count = 0;
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x)
                count += at(x, y) ? 1 : 0;
        return count;
    }
};

static void draw(Canvas& canvas, float pulse, float openness) {
    deskpet_heart::draw(64, 64, pulse, openness,
                        [&canvas](int x, int y, int w, int h) {
                            canvas.rectangle(x, y, w, h);
                        });
}

int main() {
    static_assert(deskpet_heart::kRowCount == 51,
                  "Full-size heart requires all 51 scanlines");
    static_assert(deskpet_heart::kGridWidth == 64,
                  "All rows must live in a 64-pixel grid");

    Canvas full;
    draw(full, 1.0f, 1.0f);
    assert(full.filled() > 1900);
    assert(full.filled() < 2300);

    // At the top there are two rounded lobes separated by a dip.
    // Production renders at a 1.12 vertical scale, even at rest.
    const int topY = 64 + deskpet_heart::roundToInt(
        (static_cast<int>(deskpet_heart::kFirstRow) - 32) * 1.12f);
    assert(full.at(49, topY));
    assert(full.at(78, topY));
    assert(!full.at(63, topY));
    assert(!full.at(64, topY));

    // The center fills below the notch and narrows to a pointed tip.
    // The two lobes merge 12 screen pixels below the first scanline.
    assert(full.at(63, topY + 12));
    assert(full.at(64, topY + 12));
    const int tipY = 64 + deskpet_heart::roundToInt(
        (static_cast<int>(deskpet_heart::kFirstRow) +
         static_cast<int>(deskpet_heart::kRowCount) - 1 - 32) * 1.12f);
    assert(full.at(63, tipY));
    assert(full.at(64, tipY));
    assert(!full.at(62, tipY));
    assert(!full.at(65, tipY));

    // Every pixel of the silhouette mirrors around x=63.5 exactly.
    for (int y = 0; y < Canvas::kHeight; ++y)
        for (int x = 0; x < Canvas::kWidth; ++x)
            assert(full.at(x, y) == full.at(127 - x, y));

    // A small pulse keeps the heart inside the sprite and changes area
    // without changing its recognisable shape.
    Canvas small;
    Canvas large;
    draw(small, 0.955f, 1.0f);
    draw(large, 1.045f, 1.0f);
    assert(small.filled() > 0);
    assert(large.filled() > small.filled());

    // A blinking eye compresses vertically but remains finite; an
    // entirely shut eye lets FaceAnimator draw its normal closed lid.
    Canvas blink;
    draw(blink, 1.0f, 0.50f);
    assert(blink.filled() > 0 && blink.filled() < full.filled());
    Canvas hidden;
    draw(hidden, 1.0f, 0.0f);
    assert(hidden.filled() == 0);

    puts("PASS: heart symmetry, notch, pointed tip, pulse and blink spans");
    return 0;
}
