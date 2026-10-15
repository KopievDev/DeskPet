#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "Emotion.h"
#include "IdleBehavior.h"

const char* emotionName(Emotion emotion);

// All display drawing is local: no Wi-Fi or cloud dependency.
class FaceAnimator {
public:
    explicit FaceAnimator(lgfx::LGFX_Device& display);

    // Call after display.begin(1). Uses a PSRAM-backed off-screen canvas.
    bool begin();
    void update(uint32_t nowMs);

    void setEmotion(Emotion emotion, uint32_t nowMs);
    void nextEmotion(uint32_t nowMs);
    void setSleeping(bool sleeping, uint32_t nowMs);
    bool isSleeping() const { return sleeping_; }

    // Comic quips are opt-in and never persisted by the firmware.
    void setSassEnabled(bool enabled, uint32_t nowMs);
    bool sassEnabled() const { return sassEnabled_; }
    bool showQuip(uint32_t nowMs);

    // Ambient gestures are visual-only: never replace the user's emotion.
    void setIdleGesture(IdleGesture gesture, uint32_t startedMs);

private:
    void render(float openness);
    void drawEye(int centerX, int centerY, float openness, bool isLeft);
    void drawBrow(int centerX, uint8_t index, uint16_t color);
    void drawClosedEye(int centerX, int centerY);
    void animateBrows();
    void animateEyes();
    void updateTransition(uint32_t nowMs);
    void drawQuip();
    void drawIdleDetails(int eyeY);
    float idleEnvelope() const;
    static const char* quipForEmotion(Emotion emotion);
    static bool hasSpecialEyeShape(Emotion emotion);
    static float smoothStep(float t);
    static bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs);

    struct BrowState {
        float y = 40.0f;
        float slope = 0.0f;
        float arch = 4.0f;
    };

    struct EyePose {
        float width = 72.0f;
        float height = 74.0f;
        float openness = 1.0f;
        float pupilRadius = 15.0f;
        float lidLeft = 0.0f;
        float lidRight = 0.0f;
    };

    static EyePose targetEyePose(Emotion emotion, bool isLeft);

    BrowState brows_[2]; // independent, smoothly animated left/right eyebrows
    EyePose eyes_[2];    // interpolated dimensions, eyelids and pupil sizes

    lgfx::LGFX_Device& display_;
    lgfx::LGFX_Sprite canvas_;
    Emotion emotion_ = Emotion::Neutral;
    Emotion visualEmotion_ = Emotion::Neutral;
    Emotion transitionFrom_ = Emotion::Neutral;

    int16_t width_ = 320;
    int16_t height_ = 170;
    uint32_t lastFrameMs_ = 0;
    uint32_t nextBlinkMs_ = 0;
    uint32_t blinkStartedMs_ = 0;
    uint32_t nextLookMs_ = 0;
    uint32_t emotionStartedMs_ = 0;
    uint32_t transitionStartedMs_ = 0;
    float transitionOpenness_ = 1.0f;
    float transitionStartOpenness_ = 1.0f;
    bool transitionActive_ = false;
    bool transitionRevealed_ = false;
    uint32_t animationNowMs_ = 0;
    float gazeX_ = 0.0f;
    float targetGazeX_ = 0.0f;
    bool blinking_ = false;
    bool sleeping_ = false;
    bool ready_ = false;
    IdleGesture idleGesture_ = IdleGesture::None;
    uint32_t idleStartedMs_ = 0;
    bool sassEnabled_ = false;
    bool quipVisible_ = false;
    uint32_t quipStartedMs_ = 0;
};
