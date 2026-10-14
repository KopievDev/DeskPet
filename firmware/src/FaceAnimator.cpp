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
    render(clampFloat(openness, 0.04f, 1.0f));
}

void FaceAnimator::drawEye(int centerX, int centerY, float openness) {
    const bool happy = emotion_ == Emotion::Happy && !sleeping_;
    const bool surprised = emotion_ == Emotion::Surprised && !sleeping_;
    const int eyeW = surprised ? 72 : 74;
    const int eyeH = static_cast<int>((surprised ? 98.0f : 86.0f) * openness);
    const int visibleH = eyeH < 6 ? 6 : eyeH;
    const int left = centerX - eyeW / 2;
    const int top = centerY - visibleH / 2;
    const int radius = visibleH / 2 < 22 ? visibleH / 2 : 22;

    if (happy && openness > 0.6f) {
        // Uplifted, smiling eyes: a rounded white crest with a dark lower cutout.
        canvas_.fillRoundRect(left, centerY - 18, eyeW, 37, 18, kEye);
        canvas_.fillRoundRect(left + 4, centerY - 3, eyeW - 8, 31, 15, kBackground);
        return;
    }

    canvas_.fillRoundRect(left, top, eyeW, visibleH, radius, kEye);

    if (sleeping_ || visibleH < 24 || emotion_ == Emotion::Sleepy) return;

    const int pupilX = centerX + static_cast<int>(gazeX_);
    const int pupilY = centerY + (emotion_ == Emotion::Curious ? -5 : 3);
    canvas_.fillCircle(pupilX, pupilY, surprised ? 12 : 15, kPupil);
    canvas_.fillCircle(pupilX - 5, pupilY - 6, 4, kEye);
}

void FaceAnimator::render(float openness) {
    canvas_.fillScreen(kBackground);

    const int eyeY = height_ / 2 - 4;
    drawEye(width_ / 2 - 62, eyeY, openness);
    drawEye(width_ / 2 + 62, eyeY, openness);

    // Always keep the UI legible within the 320x170 landscape LCD.
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
