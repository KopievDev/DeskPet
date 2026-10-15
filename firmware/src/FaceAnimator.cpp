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
constexpr uint32_t kSpecialTransitionMs = 360; // close old shape, open new one

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
    for (uint8_t index = 0; index < 2; ++index) {
        eyes_[index] = targetEyePose(emotion_, index == 0);
    }
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
    if (emotion == emotion_ && !sleeping_) return;

    // A second command during a transition starts from the currently
    // visible shape, not an obsolete emotion or the initial pose.
    const Emotion from = visualEmotion_;
    emotion_ = emotion;
    emotionStartedMs_ = nowMs;
    sleeping_ = false;
    blinking_ = false;
    nextBlinkMs_ = nowMs + 1300;

    // Rectangles can smoothly morph their geometry. Curved smile lines,
    // hearts and winks have a different topology: conceal the shape change
    // in a short closing/reopening eyelid animation.
    // Preserve the current lid opening if a new emotion interrupts an
    // earlier transition: no single-frame pop back to fully open eyes.
    const bool wasTransitioning = transitionActive_;
    transitionStartOpenness_ = wasTransitioning ? transitionOpenness_ : 1.0f;
    transitionActive_ = wasTransitioning || hasSpecialEyeShape(from) ||
                        hasSpecialEyeShape(emotion);
    transitionRevealed_ = false;
    transitionFrom_ = from;
    transitionStartedMs_ = nowMs;
    transitionOpenness_ = transitionStartOpenness_;
    if (!transitionActive_) visualEmotion_ = emotion;
    lastFrameMs_ = 0;
}

void FaceAnimator::nextEmotion(uint32_t nowMs) {
    const auto next = (static_cast<uint8_t>(emotion_) + 1) %
                      static_cast<uint8_t>(Emotion::Count);
    setEmotion(static_cast<Emotion>(next), nowMs);
}

void FaceAnimator::setSleeping(bool sleeping, uint32_t nowMs) {
    sleeping_ = sleeping;
    transitionActive_ = false;
    transitionOpenness_ = 1.0f;
    transitionStartOpenness_ = 1.0f;
    visualEmotion_ = emotion_;
    blinking_ = false;
    nextBlinkMs_ = nowMs + 2500;
    lastFrameMs_ = 0;
}

bool FaceAnimator::hasSpecialEyeShape(Emotion emotion) {
    return emotion == Emotion::Happy ||
           emotion == Emotion::Love ||
           emotion == Emotion::Playful;
}

float FaceAnimator::smoothStep(float t) {
    const float x = clampFloat(t, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

FaceAnimator::EyePose FaceAnimator::targetEyePose(Emotion emotion, bool isLeft) {
    EyePose pose;
    switch (emotion) {
        case Emotion::Neutral:
        case Emotion::Happy:
        case Emotion::Love:
        case Emotion::Count:
            break;
        case Emotion::Curious:
            if (isLeft) pose.lidLeft = 6.0f;
            else {
                pose.openness = 0.79f;
                pose.lidRight = 12.0f;
            }
            break;
        case Emotion::Surprised:
            pose.width = 66.0f;
            pose.height = 79.0f;
            pose.pupilRadius = 21.0f; // prominent enlarged pupils
            break;
        case Emotion::Sleepy:
            pose.openness = 0.38f;
            pose.pupilRadius = 0.0f; // pupils fade by shrinking, not disappearing
            if (isLeft) pose.lidLeft = 9.0f;
            else pose.lidRight = 9.0f;
            break;
        case Emotion::Angry:
            pose.openness = 0.83f;
            pose.pupilRadius = 12.0f;
            if (isLeft) pose.lidRight = 23.0f;
            else pose.lidLeft = 23.0f;
            break;
        case Emotion::Sad:
            pose.openness = 0.78f * (isLeft ? 0.88f : 0.95f);
            if (isLeft) pose.lidLeft = 13.0f;
            else pose.lidRight = 13.0f;
            break;
        case Emotion::Confused:
            if (isLeft) pose.lidLeft = 6.0f;
            else {
                pose.openness = 0.61f;
                pose.lidRight = 17.0f;
            }
            pose.pupilRadius = isLeft ? 15.0f : 11.0f;
            break;
        case Emotion::Excited:
            pose.width = 70.0f;
            pose.height = 78.0f;
            pose.pupilRadius = 18.0f;
            break;
        case Emotion::Playful:
            if (!isLeft) pose.lidLeft = 6.0f;
            break;
    }
    return pose;
}

void FaceAnimator::updateTransition(uint32_t nowMs) {
    if (!transitionActive_) {
        visualEmotion_ = emotion_;
        transitionOpenness_ = 1.0f;
        return;
    }

    const uint32_t elapsed = nowMs - transitionStartedMs_;
    if (elapsed >= kSpecialTransitionMs) {
        transitionActive_ = false;
        visualEmotion_ = emotion_;
        transitionOpenness_ = 1.0f;
        return;
    }

    const float phase = static_cast<float>(elapsed) / kSpecialTransitionMs;
    if (phase < 0.5f) {
        visualEmotion_ = transitionFrom_;
        transitionOpenness_ = transitionStartOpenness_ *
                               (1.0f - smoothStep(phase * 2.0f));
    } else {
        visualEmotion_ = emotion_;
        transitionOpenness_ = smoothStep((phase - 0.5f) * 2.0f);
        if (!transitionRevealed_) {
            // The eyes are closed at this instant, so switching topology
            // here is invisible. Continue opening using the destination pose.
            for (uint8_t index = 0; index < 2; ++index) {
                eyes_[index] = targetEyePose(emotion_, index == 0);
            }
            transitionRevealed_ = true;
        }
    }
}

void FaceAnimator::animateEyes() {
    // Critically damped-looking interpolation without heap allocation.
    // It remains continuous when button/Serial commands arrive rapidly.
    for (uint8_t index = 0; index < 2; ++index) {
        const EyePose target = targetEyePose(visualEmotion_, index == 0);
        EyePose& eye = eyes_[index];
        const float speed = 0.23f;
        eye.width += (target.width - eye.width) * speed;
        eye.height += (target.height - eye.height) * speed;
        eye.openness += (target.openness - eye.openness) * speed;
        eye.pupilRadius += (target.pupilRadius - eye.pupilRadius) * speed;
        eye.lidLeft += (target.lidLeft - eye.lidLeft) * speed;
        eye.lidRight += (target.lidRight - eye.lidRight) * speed;
    }
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
    updateTransition(nowMs);

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

    const float openness = sleeping_ ? 0.055f
        : (1.0f - blink * 0.94f) * transitionOpenness_;
    animateBrows();
    animateEyes();
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
    const EyePose& pose = eyes_[isLeft ? 0 : 1];
    const float eyeOpen = clampFloat(openness * pose.openness, 0.0f, 1.0f);

    // Both natural blinks and transition blinks finish in the same shape.
    if (sleeping_ || eyeOpen < 0.14f) {
        drawClosedEye(centerX, centerY);
        return;
    }

    if (visualEmotion_ == Emotion::Happy) {
        // Flatten smile arches as lids close, then restore them on opening.
        constexpr int kSpan = 30;
        for (int dx = -kSpan; dx <= kSpan; dx += 2) {
            const float t = static_cast<float>(dx) / kSpan;
            const int y = centerY +
                static_cast<int>((-15.0f + 23.0f * t * t) * eyeOpen);
            canvas_.fillCircle(centerX + dx, y, 6, kEye);
        }
        return;
    }

    if (visualEmotion_ == Emotion::Love) {
        // Heart eyes pulse while idle and shrink vertically as they blink.
        const float beat = sinf(static_cast<float>(animationNowMs_ - emotionStartedMs_) *
                                0.006f);
        const int radius = static_cast<int>((14.0f + 1.5f * (1.0f + beat)) *
                                            eyeOpen);
        const int topY = centerY - static_cast<int>(10.0f * eyeOpen);
        const int topSpan = static_cast<int>(13.0f * eyeOpen);
        const int halfBase = static_cast<int>(28.0f * eyeOpen);
        canvas_.fillCircle(centerX - topSpan, topY, radius, kAccent);
        canvas_.fillCircle(centerX + topSpan, topY, radius, kAccent);
        canvas_.fillTriangle(centerX - halfBase, topY - 3,
                             centerX + halfBase, topY - 3,
                             centerX, centerY + static_cast<int>(31.0f * eyeOpen),
                             kAccent);
        return;
    }

    if (visualEmotion_ == Emotion::Playful && isLeft) {
        // The wink flattens during transition instead of abruptly vanishing.
        for (int dx = -28; dx <= 28; dx += 2) {
            const float t = static_cast<float>(dx) / 28.0f;
            const int y = centerY + static_cast<int>(
                (5.0f + 5.0f * t * t + 3.0f * t) * eyeOpen);
            canvas_.fillCircle(centerX + dx, y,
                               2 + static_cast<int>(2.0f * eyeOpen), kEye);
        }
        return;
    }

    // All remaining expressions use the same parameterized silhouette.
    // Width, height, opening, pupil size and each sloping eyelid morph
    // continuously, including interruptions by rapid new commands.
    const bool surprised = visualEmotion_ == Emotion::Surprised;
    const bool confused = visualEmotion_ == Emotion::Confused;
    const bool excited = visualEmotion_ == Emotion::Excited;
    const bool curious = visualEmotion_ == Emotion::Curious;
    const bool sad = visualEmotion_ == Emotion::Sad;

    const int eyeW = static_cast<int>(pose.width + 0.5f);
    const int eyeH = static_cast<int>(pose.height * eyeOpen);
    const int visibleH = eyeH < 6 ? 6 : eyeH;
    const int bounce = excited
        ? static_cast<int>(2.5f * sinf(
              static_cast<float>(animationNowMs_ - emotionStartedMs_) * 0.011f))
        : 0;
    const int left = centerX - eyeW / 2;
    const int top = centerY - visibleH / 2 + bounce;
    const int radius = visibleH / 2 < eyeW / 2 ? visibleH / 2 : eyeW / 2;

    canvas_.fillRoundRect(left, top, eyeW, visibleH, radius, kEye);

    // Shrink pupils as Sleepy closes its eyelids; clip their radius
    // to the available eye height to prevent dark circles spilling out.
    const int requestedRadius = static_cast<int>(pose.pupilRadius + 0.5f);
    const int availableRadius = visibleH / 2 - 3;
    const int pupilRadius = requestedRadius < availableRadius
                                ? requestedRadius : availableRadius;
    if (visibleH >= 20 && pupilRadius >= 3) {
        // Large surprised pupils cannot cross the white boundary.
        const int lookOffset = confused ? (isLeft ? -8 : 7)
            : surprised ? static_cast<int>(clampFloat(gazeX_, -8.0f, 8.0f))
                        : static_cast<int>(gazeX_);
        const int pupilX = centerX + lookOffset;
        const int pupilY = centerY + bounce + ((curious || sad) ? -2 : 3);
        canvas_.fillCircle(pupilX, pupilY, pupilRadius, kPupil);
        if (pupilRadius >= 8) {
            canvas_.fillCircle(pupilX - 5, pupilY - 6, surprised ? 6 : 4, kEye);
        }
        if (excited) {
            canvas_.fillCircle(pupilX + 7, pupilY + 8, 2, kEye);
        }
    }

    // Two continuous lid-cut parameters replace hard-coded emotion masks.
    // Avoid cutting a still-closed sliver; the full closed eye is handled
    // above. Both cuts can animate independently without switching shapes.
    if (visibleH > 14) {
        const int cutLeft = static_cast<int>(pose.lidLeft * eyeOpen + 0.5f);
        const int cutRight = static_cast<int>(pose.lidRight * eyeOpen + 0.5f);
        if (cutLeft > 0) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left, top + cutLeft, kBackground);
        }
        if (cutRight > 0) {
            canvas_.fillTriangle(left, top, left + eyeW, top,
                                 left + eyeW, top + cutRight, kBackground);
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

    // Decorations enter only with the visible shape, not at the start of
    // a topology-changing blink.
    if (!sleeping_ && openness > 0.35f) {
        if (visualEmotion_ == Emotion::Happy || visualEmotion_ == Emotion::Love) {
            canvas_.fillCircle(leftEyeX - 47, eyeY + 13, 3, kAccent);
            canvas_.fillCircle(rightEyeX + 47, eyeY + 13, 3, kAccent);
        } else if (visualEmotion_ == Emotion::Sad) {
            // A single falling tear; position is always above the HUD.
            const int drop = static_cast<int>(
                ((animationNowMs_ - emotionStartedMs_) / 45u) % 12u);
            canvas_.fillCircle(rightEyeX + 29, eyeY + 25 + drop, 3, kAccent);
            canvas_.fillTriangle(rightEyeX + 26, eyeY + 24 + drop,
                                 rightEyeX + 32, eyeY + 24 + drop,
                                 rightEyeX + 29, eyeY + 17 + drop, kAccent);
        } else if (visualEmotion_ == Emotion::Excited) {
            // Twin sparkles at the outer edges, blinking independently.
            const int spark = (animationNowMs_ / 180u) % 2u == 0u ? 6 : 3;
            for (int side = -1; side <= 1; side += 2) {
                const int sx = width_ / 2 + side * 113;
                const int sy = eyeY - 18;
                canvas_.fillRect(sx - 1, sy - spark, 3, 2 * spark + 1, kAccent);
                canvas_.fillRect(sx - spark, sy - 1, 2 * spark + 1, 3, kAccent);
            }
        } else if (visualEmotion_ == Emotion::Playful) {
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
