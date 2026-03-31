#include "audio.h"

void Audio::begin() {
    ledcAttach(BUZZER_PIN, 1000, 8);
    ledcWrite(BUZZER_PIN, 0);
}

void Audio::toggle() {
    _enabled = !_enabled;
    if (!_enabled) {
        ledcWrite(BUZZER_PIN, 0);
        _toneActive = false;
    }
}

void Audio::update(int32_t rssi) {
    if (_toneActive && (millis() - _toneStartMs >= TONE_DURATION_MS)) {
        ledcWrite(BUZZER_PIN, 0);
        _toneActive = false;
    }

    if (!_enabled) return;

    unsigned long now = millis();
    unsigned int interval = beepIntervalMs(rssi);

    if (now - _lastBeepMs >= interval) {
        _lastBeepMs = now;
        int freq = map(constrain(rssi, -100, -30), -100, -30, 800, 2400);
        ledcWriteTone(BUZZER_PIN, freq);
        _toneActive = true;
        _toneStartMs = now;
    }
}

unsigned int Audio::beepIntervalMs(int32_t rssi) {
    return map(constrain(rssi, -100, -30), -100, -30, 1200, 80);
}
