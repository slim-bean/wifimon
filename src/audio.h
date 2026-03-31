#pragma once
#include <Arduino.h>

static constexpr uint8_t BUZZER_PIN = GPIO_NUM_2;

class Audio {
public:
    void begin();
    void setEnabled(bool on) { _enabled = on; }
    bool isEnabled() const { return _enabled; }
    void toggle();

    void update(int32_t rssi);

private:
    bool _enabled = false;
    bool _toneActive = false;
    unsigned long _lastBeepMs = 0;
    unsigned long _toneStartMs = 0;
    static constexpr unsigned long TONE_DURATION_MS = 30;
    unsigned int beepIntervalMs(int32_t rssi);
};
