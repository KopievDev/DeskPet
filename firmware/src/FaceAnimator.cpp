#include "FaceAnimator.h"

#include <math.h>

namespace {
constexpr uint16_t kBackground = 0x0862;
constexpr uint16_t kEye = 0xDFFF;
constexpr uint16_t kPupil = 0x0882;
constexpr uint16_t kAccent = 0x5F1F;
constexpr uint16_t kMuted = 0x94B2;
constexpr uint32_t kFrameIntervalMs = 33;
constexpr uint32_t kBlinkDurationMs = 180;

float clampFloat(float value, float minValue, float maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}
} // namespace

const char* emotionName(Emotion emotion) {
    switch (emotion) {
        case Emotion::Neutral: return "NEUTRAL";
        case Emotion::Happy: return "HAPPY";
        case Emotion::Curious: return "CURIOUS";
        case Emotion::Surprised: return "SURPRISED";
        case Emotion::Sleepy: return "SLEEPY";
        case Emotion::Count: break;
    }
    return "UNKNOWN";
}

FaceAnimator::FaceAnimator(lgfx::LGFX_Device& display)
    : display_(display), canvas_(&display) {}

bool FaceAnimator::begin() {
    width_ = display_.width();
    height_ = display_.height();
    canvas_.setColorDepth(16);
    canvas_.setPsram(true);
    ready_ = canvas_.createSprite(width_, height_) != nullptr;
    if (!ready_) {
        display_.fillScreen(0xF800);
        return false;
    }

    const uint32_t now = millis();
    nextBlinkMs_ = now + 1800;
    nextLookMs_ = now + 1200;
    render(1.0f);
    return true;
}

bool FaceAnimator::deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

void FaceAnimator::setEmotion(Emotion emotion, uint32_t nowMs) {
    if (emotion == Emotion::Count) return;
    emotion_ = emotion;
    sleeping_ = false;
    blinking_ = false;
    nextBlinkMs_ = nowMs + 1300;
    lastFrameMs_ = 0; // immediately display new state on next update
}

void FaceAnimator::nextEmotion(uint32_t nowMs) {
    const auto next = (static_cast<uint8_t>(emotion_) + 1) %
                      static_cast<uint8_t>(Emotion::Count);
    setEmotion(static_cast<Emotion>(next), nowMs);
}

void FaceAnimator::setSleeping(bool sleeping, uint32_t nowMs) {
    sleeping_ = sleeping;
    blinking_ = false;
    nextBlinkMs_ = nowMs + 2500;
    lastFrameMs_ = 0;
}

void FaceAnimator::animateBrows() {
    // Target poses are tuned for the 320x170 LCD: eyebrows remain above
    // even the tallest eye, and asymmetry makes CURIOUS visibly different.
    float y[2] = {42.0f, 42.0f};
    float slope[2] = {-2.0f, 2.0f};
    float arch[2] = {3.0f, 3.0f};

    if (sleeping_) {
        y[0] = y[1] = 56.0f;
        slope[0] = 3.0f;
        slope[1] = -3.0f;
        arch[0] = arch[1] = 1.0f;
    } else {
        switch (emotion_) {
            case Emotion::Happy:
                y[0] = y[1] = 43.0f;
                slope[0] = -3.0f;
                slope[1] = 3.0f;
                arch[0] = arch[1] = 8.0f;
                break;
            case Emotion::Curious:
                y[0] = 34.0f;
                y[1] = 47.0f;
                slope[0] = -8.0f;
                slope[1] = 6.0f;
                arch[0] = 5.0f;
                arch[1] = 2.0f;
                break;
            case Emotion::Surprised:
                y[0] = y[1] = 32.0f;
                slope[0] = slope[1] = 0.0f;
                arch[0] = arch[1] = 7.0f;
                break;
            case Emotion::Sleepy:
                y[0] = y[1] = 53.0f;
                slope[0] = 4.0f;
                slope[1] = -4.0f;
                arch[0] = arch[1] = 1.0f;
                break;
            case Emotion::Neutral:
            case Emotion::Count:
                break;
        }
    }

    // Smooth positional transitions instead of brows jumping on a button
    // press. The render loop is capped at ~30 FPS.
    for (uint8_t index = 0; index < 2; ++index) {
        brows_[index].y += (y[index] - brows_[index].y) * 0.19f;
        brows_[index].slope += (slope[index] - brows_[index].slope) * 0.19f;
        brows_[index].arch += (arch[index] - brows_[index].arch) * 0.19f;
    }
}

void FaceAnimator::update(uint32_t nowMs) {
    if (!ready_) return;
    if (lastFrameMs_ != 0 && nowMs - lastFrameMs_ < kFrameIntervalMs) return;
    lastFrameMs_ = nowMs;

    if (!sleeping_ && deadlineReached(nowMs, nextLookMs_)) {
        targetGazeX_ = static_cast<float>(random(-13, 14));
        nextLookMs_ = nowMs + static_cast<uint32_t>(random(1200, 3600));
    }
    gazeX_ += (targetGazeX_ - gazeX_) * 0.075f;

    float blink = 0.0f;
    if (!sleeping_) {
        if (!blinking_ && deadlineReached(nowMs, nextBlinkMs_)) {
            blinking_ = true;
            blinkStartedMs_ = nowMs;
        }
        if (blinking_) {
            const uint32_t elapsed = nowMs - blinkStartedMs_;
            if (elapsed >= kBlinkDurationMs) {
                blinking_ = false;
                nextBlinkMs_ = nowMs + static_cast<uint32_t>(random(1900, 5200));
            } else {
                blink = sinf(3.14159265f *
                             static_cast<float>(elapsed) / kBlinkDurationMs);
            }
        }
    }

    float openness = 1.0f;
    if (sleeping_) openness = 0.055f;
    else if (emotion_ == Emotion::Sleepy) openness = 0.38f;
    openness *= 1.0f - blink * 0.94f;
    animateBrows();
    render(clampFloat(openness, 0.04f, 1.0f));
}

void FaceAnimator::drawClosedEye(int centerX, int centerY, float openness) {
    // A slightly softened horizontal line works for a full blink and sleep.
    // It also avoids the old "tiny rectangular pupil" at low openness.
    const int stroke = sleeping_ ? 4 : 5;
    const int y = sleeping_ ? centerY + 5 : centerY;
    canvas_.fillRoundRect(centerX - 31, y - stroke / 2,
                          62, stroke, stroke / 2, kEye);
}

void FaceAnimator::drawEye(int centerX, int centerY, float openness, bool isLeft) {
    if (sleeping_ || openness < 0.14f) {
        drawClosedEye(centerX, centerY, openness);
        return;
    }

    if (emotion_ == Emotion::Happy) {
        // Smiling eyes are smooth upside-down U arcs (not hollow rectangles).
        // Scale the curvature with blink openness so the arc gently flattens
        // instead of snapping into an unrelated rectangle while blinking.
        const int span = 30;
        const int stroke = 6;
        for (int dx = -span; dx <= span; dx += 2) {
            const float t = static_cast<float>(dx) / span;
            const float arcY = -15.0f + 23.0f * t * t;
            const int y = centerY + static_cast<int>(arcY * openness);
            canvas_.fillCircle(centerX + dx, y, stroke, kEye);
        }
        return;
    }

    const bool surprised = emotion_ == Emotion::Surprised;
    const bool curious = emotion_ == Emotion::Curious;
    const bool sleepy = emotion_ == Emotion::Sleepy;

    // Curious eyes react differently to their brows: one wide and alert,
    // the other slightly narrowed by the upper lid.
    const float asymmetricOpen = curious && !isLeft ? 0.79f : 1.0f;
    const int eyeW = surprised ? 66 : 72;
    const int fullHeight = surprised ? 79 : 74;
    const int eyeH = static_cast<int>(fullHeight * openness * asymmetricOpen);
    const int visibleH = eyeH < 6 ? 6 : eyeH;
    const int left = centerX - eyeW / 2;
    const int top = centerY - visibleH / 2;
    const int radius = visibleH / 2 < eyeW / 2
                           ? visibleH / 2 : eyeW / 2;

    canvas_.fillRoundRect(left, top, eyeW, visibleH, radius, kEye);

    // A half-closed sleepy eye reads as an eyelid, not as a tiny iris.
    if (!sleepy && visibleH >= 24) {
        const int pupilX = centerX + static_cast<int>(gazeX_);
        const int pupilY = centerY + (curious ? -2 : 3);
        const int pupilRadius = surprised ? 12 : 15;
        canvas_.fillCircle(pupilX, pupilY, pupilRadius, kPupil);
        canvas_.fillCircle(pupilX - 5, pupilY - 6, 4, kEye);
    }

    if (curious && visibleH > 25) {
        // Remove a small diagonal wedge from the upper edge to make the
        // upper eyelid follow the lowered eyebrow on the right.
        if (isLeft) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + 6, kBackground);
        } else {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + 12, kBackground);
        }
    } else if (sleepy && visibleH > 14) {
        // The softly sloped upper eyelids also echo the drooping brows.
        const int wedge = 9;
        if (isLeft) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + wedge, kBackground);
        } else {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + wedge, kBackground);
        }
    }
}

void FaceAnimator::drawBrow(int centerX, uint8_t index, uint16_t color) {
    // Draw a soft curved stroke with circular caps. Positive arch raises
    // the middle; slope controls the inner/outer ends independently.
    const BrowState& brow = brows_[index];
    constexpr int kHalfWidth = 25;
    constexpr int kRadius = 2;
    for (int dx = -kHalfWidth; dx <= kHalfWidth; dx += 2) {
        const float t = static_cast<float>(dx) / kHalfWidth;
        const float y = brow.y + brow.slope * t +
                        brow.arch * (t * t - 1.0f);
        canvas_.fillCircle(centerX + dx, static_cast<int>(y),
                           kRadius, color);
    }
}

void FaceAnimator::render(float openness) {
    canvas_.fillScreen(kBackground);

    // Move the eyes slightly down: a dedicated brow band is now visible
    // between the top status line and the eyes on a 320x170 IPS display.
    const int eyeY = height_ / 2 + 10;
    const int leftEyeX = width_ / 2 - 62;
    const int rightEyeX = width_ / 2 + 62;
    drawEye(leftEyeX, eyeY, openness, true);
    drawEye(rightEyeX, eyeY, openness, false);

    const bool expressive = emotion_ == Emotion::Happy ||
                            emotion_ == Emotion::Surprised ||
                            emotion_ == Emotion::Curious;
    const uint16_t browColor = expressive && !sleeping_ ? kAccent : kMuted;
    drawBrow(leftEyeX, 0, browColor);
    drawBrow(rightEyeX, 1, browColor);

    // Small accents at the outer cheeks only for the smiling expression.
    if (emotion_ == Emotion::Happy && !sleeping_ && openness > 0.35f) {
        canvas_.fillCircle(leftEyeX - 47, eyeY + 13, 3, kAccent);
        canvas_.fillCircle(rightEyeX + 47, eyeY + 13, 3, kAccent);
    }

    // Keep the original HUD legible and clear of the face geometry.
    canvas_.fillCircle(15, 13, 4, sleeping_ ? kMuted : kAccent);
    canvas_.setTextColor(kMuted, kBackground);
    canvas_.setTextSize(1);
    canvas_.setCursor(26, 9);
    canvas_.print("DeskPet");
    canvas_.setCursor(10, height_ - 17);
    canvas_.print(sleeping_ ? "SLEEPING" : emotionName(emotion_));
    canvas_.setCursor(width_ - 80, height_ - 17);
    canvas_.print("v0.1");

    canvas_.pushSprite(0, 0);
}
