#pragma once
#include <M5StickCPlus.h>
#include "wifi_scanner.h"

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 135;

enum class AppScreen {
    NETWORK_LIST,
    MONITOR
};

class Display {
public:
    void begin();
    void drawNetworkList(const ScanResult& result, int selectedIndex);
    void drawMonitor(const WifiScanner& scanner, bool buzzerOn);
    void drawSplash();
    void drawScanning();

private:
    TFT_eSprite _sprite = TFT_eSprite(&M5.Lcd);
    bool _spriteCreated = false;

    void ensureSprite();
    uint16_t rssiColor(int32_t rssi);
    const char* rssiLabel(int32_t rssi);
    void drawSignalBar(int x, int y, int w, int h, int32_t rssi);
    void drawHistoryGraph(int x, int y, int w, int h, const WifiScanner& scanner);
    void drawBattery(int x, int y);
};
