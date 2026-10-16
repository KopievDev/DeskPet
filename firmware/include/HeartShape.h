#pragma once

#include <stdint.h>

namespace deskpet_heart {

// Centered, symmetric 64-column heart silhouette, hand-tuned to resemble
// the classic mathematical heart curve: round lobes, a genuine notch,
// smoothly narrowing sides and one pointed tip.
// One row stores the leftmost pixel and, for the top ten rows,
// the innermost left-lobe pixel. The right side is mirrored at draw time.
// This compact lookup avoids filling hundreds of pixels with float
// implicit-equation tests on every frame of the ESP32-S3 display.
struct Row {
    uint8_t left;
    uint8_t notchLeft; // 0 means the two lobes have joined
};

constexpr uint8_t kFirstRow = 7;
constexpr uint8_t kGridWidth = 64;
constexpr Row kRows[] = {
    {14, 21},
    {12, 23},
    {10, 25},
    {8, 26},
    {7, 27},
    {6, 28},
    {6, 29},
    {5, 29},
    {5, 30},
    {4, 30},
    {4, 0},
    {4, 0},
    {3, 0},
    {3, 0},
    {3, 0},
    {3, 0},
    {3, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {5, 0},
    {5, 0},
    {6, 0},
    {6, 0},
    {7, 0},
    {8, 0},
    {9, 0},
    {10, 0},
    {11, 0},
    {12, 0},
    {12, 0},
    {14, 0},
    {15, 0},
    {16, 0},
    {17, 0},
    {18, 0},
    {19, 0},
    {21, 0},
    {22, 0},
    {23, 0},
    {24, 0},
    {25, 0},
    {26, 0},
    {27, 0},
    {28, 0},
    {29, 0},
    {29, 0},
    {30, 0},
    {31, 0},
    {31, 0},
    {31, 0},
};
constexpr uint8_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);

// Round to nearest integer without relying on platform-specific libm.
inline int roundToInt(float value) {
    return static_cast<int>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

// Callback signature: drawRect(x, y, width, height).
// Templated callback keeps the renderer allocation-free. Center is the
// center of a 64-pixel logical cell grid; span ends are exclusive.
template <typename DrawRect>
void draw(int centerX, int centerY, float size, float openness,
          DrawRect drawRect) {
    if (size <= 0.0f || openness <= 0.0f) return;
    if (openness > 1.0f) openness = 1.0f;

    const float scaleX = size;
    const float scaleY = size * 1.12f * openness;
    for (uint8_t rowIndex = 0; rowIndex < kRowCount; ++rowIndex) {
        const Row& line = kRows[rowIndex];
        const int rowY = static_cast<int>(kFirstRow) + rowIndex;
        const int y0 = centerY + roundToInt((rowY - 32) * scaleY);
        const int y1 = centerY + roundToInt((rowY + 1 - 32) * scaleY);
        if (y1 <= y0) continue;

        const int left = centerX + roundToInt((line.left - 32) * scaleX);
        const int right = centerX + roundToInt((32 - line.left) * scaleX);
        if (right <= left) continue;

        if (line.notchLeft != 0u) {
            const int leftEnd =
                centerX + roundToInt((line.notchLeft + 1 - 32) * scaleX);
            const int rightStart =
                centerX + roundToInt((31 - line.notchLeft) * scaleX);
            if (leftEnd > left) drawRect(left, y0, leftEnd - left, y1 - y0);
            if (right > rightStart) {
                drawRect(rightStart, y0, right - rightStart, y1 - y0);
            }
        } else {
            drawRect(left, y0, right - left, y1 - y0);
        }
    }
}

} // namespace deskpet_heart
