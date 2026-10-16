#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

// LILYGO T-Display-S3 (standard 1.9" LCD, not AMOLED / Touch / Pro).
//
// Pin mapping and ST7789 panel offsets follow LILYGO's public board profile:
// https://github.com/Xinyuan-LILYGO/LilyGo-display-library
//
// The vendor profile assigns cfg.i2s_port = I2S_NUM_0. On ESP32-S3,
// LovyanGFX 1.2.33 implements Bus_Parallel8 with the LCD_CAM peripheral
// and config_t::port instead. Keeping our minimal board adapter here makes
// the firmware compile without editing a downloaded dependency.
class DeskPetTDisplayS3 final : public lgfx::LGFX_Device {
public:
    DeskPetTDisplayS3() {
        {
            auto cfg = bus_.config();
            cfg.port = 0; // ESP32-S3 has one LCD_CAM instance.
            cfg.freq_write = 20000000;
            cfg.pin_wr = 8;
            cfg.pin_rd = 9;
            cfg.pin_rs = 7;
            cfg.pin_d0 = 39;
            cfg.pin_d1 = 40;
            cfg.pin_d2 = 41;
            cfg.pin_d3 = 42;
            cfg.pin_d4 = 45;
            cfg.pin_d5 = 46;
            cfg.pin_d6 = 47;
            cfg.pin_d7 = 48;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }

        {
            auto cfg = panel_.config();
            cfg.pin_cs = 6;
            cfg.pin_rst = 5;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 170;
            cfg.panel_height = 320;
            cfg.offset_x = 35;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            panel_.config(cfg);
        }

        {
            auto cfg = light_.config();
            cfg.pin_bl = 38;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            light_.config(cfg);
            panel_.setLight(&light_);
        }

        setPanel(&panel_);
    }

    bool init() {
        // LCD power enable; do this before initializing the display bus.
        lgfx::pinMode(15, lgfx::pin_mode_t::output);
        lgfx::gpio_hi(15);
        return lgfx::LGFX_Device::init();
    }

    bool begin(uint8_t rotation = 1, uint8_t brightness = 255) {
        if (!init()) return false;
        setRotation(rotation);
        setBrightness(brightness);
        fillScreen(TFT_BLACK);
        return true;
    }

private:
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_Parallel8 bus_;
    lgfx::Light_PWM light_;
};
