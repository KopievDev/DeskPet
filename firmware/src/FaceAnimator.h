#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

enum class Emotion : uint8_t {
    Neutral = 0,
    Happy,
    Curious,
    Surprised,
    Sleepy,
    Count
};

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

private:
    void render(float openness);
    void drawEye(int centerX, int centerY, float openness);
    static bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs);

    lgfx::LGFX_Device& display_;
    lgfx::LGFX_Sprite canvas_;
    Emotion emotion_ = Emotion::Neutral;

    int16_t width_ = 320;
    int16_t height_ = 170;
    uint32_t lastFrameMs_ = 0;
    uint32_t nextBlinkMs_ = 0;
    uint32_t blinkStartedMs_ = 0;
    uint32_t nextLookMs_ = 0;
    float gazeX_ = 0.0f;
    float targetGazeX_ = 0.0f;
    bool blinking_ = false;
    bool sleeping_ = false;
    bool ready_ = false;
};
