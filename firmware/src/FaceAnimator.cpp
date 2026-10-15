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
        case Emotion::Angry: return "ANGRY";
        case Emotion::Sad: return "SAD";
        case Emotion::Love: return "LOVE";
        case Emotion::Confused: return "CONFUSED";
        case Emotion::Excited: return "EXCITED";
        case Emotion::Playful: return "PLAYFUL";
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
    emotionStartedMs_ = now;
    animationNowMs_ = now;
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
    emotionStartedMs_ = nowMs;
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
            case Emotion::Angry:
                y[0] = y[1] = 48.0f;
                slope[0] = 11.0f;  // brow tips slope toward the nose
                slope[1] = -11.0f;
                arch[0] = arch[1] = 1.0f;
                break;
            case Emotion::Sad:
                y[0] = y[1] = 45.0f;
                slope[0] = -8.0f;  // raised inner corners
                slope[1] = 8.0f;
                arch[0] = arch[1] = -3.0f;
                break;
            case Emotion::Love:
                y[0] = y[1] = 41.0f;
                slope[0] = -3.0f;
                slope[1] = 3.0f;
                arch[0] = arch[1] = 9.0f;
                break;
            case Emotion::Confused:
                y[0] = 35.0f;
                y[1] = 53.0f;
                slope[0] = -9.0f;
                slope[1] = -4.0f;
                arch[0] = 5.0f;
                arch[1] = 0.0f;
                break;
            case Emotion::Excited:
                y[0] = y[1] = 32.0f;
                slope[0] = -4.0f;
                slope[1] = 4.0f;
                arch[0] = arch[1] = 8.0f;
                break;
            case Emotion::Playful:
                y[0] = 51.0f;
                y[1] = 39.0f;
                slope[0] = 6.0f;
                slope[1] = 4.0f;
                arch[0] = 2.0f;
                arch[1] = 6.0f;
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
    animationNowMs_ = nowMs;

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
    else if (emotion_ == Emotion::Angry) openness = 0.83f;
    else if (emotion_ == Emotion::Sad) openness = 0.78f;
    openness *= 1.0f - blink * 0.94f;
    animateBrows();
    render(clampFloat(openness, 0.04f, 1.0f));
}

void FaceAnimator::drawClosedEye(int centerX, int centerY) {
    // A slightly softened horizontal line works for a full blink and sleep.
    // It also avoids the old "tiny rectangular pupil" at low openness.
    const int stroke = sleeping_ ? 4 : 5;
    const int y = sleeping_ ? centerY + 5 : centerY;
    canvas_.fillRoundRect(centerX - 31, y - stroke / 2,
                          62, stroke, stroke / 2, kEye);
}

void FaceAnimator::drawEye(int centerX, int centerY, float openness, bool isLeft) {
    // All expressive shapes flatten to a simple lid during a full blink.
    if (sleeping_ || openness < 0.14f) {
        drawClosedEye(centerX, centerY);
        return;
    }

    if (emotion_ == Emotion::Happy) {
        // Smooth, upward smiling eyelids, instead of hollow rectangles.
        constexpr int kSpan = 30;
        for (int dx = -kSpan; dx <= kSpan; dx += 2) {
            const float t = static_cast<float>(dx) / kSpan;
            const int y = centerY + static_cast<int>((-15.0f + 23.0f * t * t) * openness);
            canvas_.fillCircle(centerX + dx, y, 6, kEye);
        }
        return;
    }

    if (emotion_ == Emotion::Love) {
        // Pulsing heart eyes. Keep the entire silhouette inside 320x170.
        const float beat = sinf(static_cast<float>(animationNowMs_ - emotionStartedMs_) *
                                0.006f);
        const int radius = 14 + static_cast<int>(1.5f * (1.0f + beat));
        const int topY = centerY - 10;
        canvas_.fillCircle(centerX - 13, topY, radius, kAccent);
        canvas_.fillCircle(centerX + 13, topY, radius, kAccent);
        canvas_.fillTriangle(centerX - 28, topY - 3,
                             centerX + 28, topY - 3,
                             centerX, centerY + 31, kAccent);
        return;
    }

    if (emotion_ == Emotion::Playful && isLeft) {
        // The left eye stays in a crooked wink, the right looks at the user.
        for (int dx = -28; dx <= 28; dx += 2) {
            const float t = static_cast<float>(dx) / 28.0f;
            const int y = centerY + 5 + static_cast<int>(5.0f * t * t + 3.0f * t);
            canvas_.fillCircle(centerX + dx, y, 4, kEye);
        }
        return;
    }

    const bool surprised = emotion_ == Emotion::Surprised;
    const bool curious = emotion_ == Emotion::Curious;
    const bool sleepy = emotion_ == Emotion::Sleepy;
    const bool angry = emotion_ == Emotion::Angry;
    const bool sad = emotion_ == Emotion::Sad;
    const bool confused = emotion_ == Emotion::Confused;
    const bool excited = emotion_ == Emotion::Excited;
    const bool playful = emotion_ == Emotion::Playful;

    float asymmetricOpen = curious && !isLeft ? 0.79f : 1.0f;
    if (confused && !isLeft) asymmetricOpen = 0.61f;
    if (sad) asymmetricOpen = isLeft ? 0.88f : 0.95f;

    const int eyeW = surprised ? 66 : (excited ? 70 : 72);
    const int fullHeight = surprised ? 79 : (excited ? 78 : 74);
    const int eyeH = static_cast<int>(fullHeight * openness * asymmetricOpen);
    const int visibleH = eyeH < 6 ? 6 : eyeH;
    const int bounce = excited
        ? static_cast<int>(2.5f * sinf(static_cast<float>(animationNowMs_ - emotionStartedMs_) * 0.011f))
        : 0;
    const int left = centerX - eyeW / 2;
    const int top = centerY - visibleH / 2 + bounce;
    const int radius = visibleH / 2 < eyeW / 2
                           ? visibleH / 2 : eyeW / 2;

    canvas_.fillRoundRect(left, top, eyeW, visibleH, radius, kEye);

    // No floating pupil inside a narrow drowsy eyelid.
    if (!sleepy && visibleH >= 24) {
        int pupilRadius = 15;
        if (surprised) pupilRadius = 21;  // deliberately much larger than neutral
        if (angry) pupilRadius = 12;
        if (confused) pupilRadius = isLeft ? 15 : 11;
        if (excited) pupilRadius = 18;
        const int lookOffset = confused ? (isLeft ? -8 : 7) : static_cast<int>(gazeX_);
        const int pupilX = centerX + lookOffset;
        const int pupilY = centerY + bounce + ((curious || sad) ? -2 : 3);
        canvas_.fillCircle(pupilX, pupilY, pupilRadius, kPupil);
        canvas_.fillCircle(pupilX - 5, pupilY - 6, surprised ? 6 : 4, kEye);
        if (excited) {
            canvas_.fillCircle(pupilX + 7, pupilY + 8, 2, kEye);
        }
    }

    // Emotion-specific upper lids. Triangular background masks never
    // touch neighboring eyes or the status text.
    if ((curious || confused) && visibleH > 25) {
        if (isLeft) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + 6, kBackground);
        } else {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + (confused ? 17 : 12), kBackground);
        }
    } else if (sleepy && visibleH > 14) {
        if (isLeft) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + 9, kBackground);
        } else {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + 9, kBackground);
        }
    } else if (angry && visibleH > 25) {
        // Inner edges drop down, making a recognizable angry / shape.
        if (isLeft) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + 23, kBackground);
        } else {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + 23, kBackground);
        }
    } else if (sad && visibleH > 25) {
        // Opposite of angry: inner corners lift, outer corners droop.
        if (isLeft) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + 13, kBackground);
        } else {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + 13, kBackground);
        }
    } else if (playful && visibleH > 25) {
        canvas_.fillTriangle(left, top, left + eyeW, top,
                             left, top + 6, kBackground);
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

    const int eyeY = height_ / 2 + 10;
    const int leftEyeX = width_ / 2 - 62;
    const int rightEyeX = width_ / 2 + 62;
    drawEye(leftEyeX, eyeY, openness, true);
    drawEye(rightEyeX, eyeY, openness, false);

    const bool expressive = emotion_ != Emotion::Neutral &&
                            emotion_ != Emotion::Sleepy && !sleeping_;
    const uint16_t browColor = expressive ? kAccent : kMuted;
    drawBrow(leftEyeX, 0, browColor);
    drawBrow(rightEyeX, 1, browColor);

    // A few inexpensive visual accents keep expressive moods distinct.
    if (!sleeping_ && openness > 0.35f) {
        if (emotion_ == Emotion::Happy || emotion_ == Emotion::Love) {
            canvas_.fillCircle(leftEyeX - 47, eyeY + 13, 3, kAccent);
            canvas_.fillCircle(rightEyeX + 47, eyeY + 13, 3, kAccent);
        } else if (emotion_ == Emotion::Sad) {
            // A single falling tear; position is always above the HUD.
            const int drop = static_cast<int>(
                ((animationNowMs_ - emotionStartedMs_) / 45u) % 12u);
            canvas_.fillCircle(rightEyeX + 29, eyeY + 25 + drop, 3, kAccent);
            canvas_.fillTriangle(rightEyeX + 26, eyeY + 24 + drop,
                                 rightEyeX + 32, eyeY + 24 + drop,
                                 rightEyeX + 29, eyeY + 17 + drop, kAccent);
        } else if (emotion_ == Emotion::Excited) {
            // Twin sparkles at the outer edges, blinking independently.
            const int spark = (animationNowMs_ / 180u) % 2u == 0u ? 6 : 3;
            for (int side = -1; side <= 1; side += 2) {
                const int sx = width_ / 2 + side * 113;
                const int sy = eyeY - 18;
                canvas_.fillRect(sx - 1, sy - spark, 3, 2 * spark + 1, kAccent);
                canvas_.fillRect(sx - spark, sy - 1, 2 * spark + 1, 3, kAccent);
            }
        } else if (emotion_ == Emotion::Playful) {
            canvas_.fillCircle(leftEyeX - 44, eyeY + 9, 3, kAccent);
        }
    }

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
