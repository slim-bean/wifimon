#include "display.h"

void Display::begin() {
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
}

void Display::ensureSprite() {
    if (!_spriteCreated) {
        _sprite.createSprite(SCREEN_W, SCREEN_H);
        _sprite.setSwapBytes(true);
        _spriteCreated = true;
    }
}

void Display::drawSplash() {
    ensureSprite();
    _sprite.fillSprite(BLACK);
    _sprite.setTextColor(TFT_CYAN);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.setTextSize(2);
    _sprite.drawString("WiFiMon", SCREEN_W / 2, SCREEN_H / 2 - 10);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_DARKGREY);
    _sprite.drawString("Scanning networks...", SCREEN_W / 2, SCREEN_H / 2 + 15);
    _sprite.pushSprite(0, 0);
}

void Display::drawScanning() {
    ensureSprite();
    _sprite.fillSprite(BLACK);
    _sprite.setTextColor(TFT_YELLOW);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.setTextSize(1);
    _sprite.drawString("Scanning...", SCREEN_W / 2, SCREEN_H / 2);
    _sprite.pushSprite(0, 0);
}

void Display::drawNetworkList(const ScanResult& result, int selectedIndex) {
    ensureSprite();
    _sprite.fillSprite(BLACK);

    _sprite.setTextDatum(TL_DATUM);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_CYAN);
    _sprite.drawString("Select Network (A=sel B=next)", 2, 2);

    drawBattery(SCREEN_W - 28, 2);

    int maxVisible = 7;
    int startIdx = 0;
    if (selectedIndex >= maxVisible) {
        startIdx = selectedIndex - maxVisible + 1;
    }

    for (int i = 0; i < maxVisible && (startIdx + i) < result.count; i++) {
        int idx = startIdx + i;
        int y = 16 + i * 16;
        bool selected = (idx == selectedIndex);

        if (selected) {
            _sprite.fillRect(0, y, SCREEN_W, 15, TFT_NAVY);
        }

        int32_t rssi = result.networks[idx].rssi;
        _sprite.setTextColor(rssiColor(rssi));

        char line[48];
        snprintf(line, sizeof(line), "%s [%d] %ddBm",
                 result.networks[idx].ssid.c_str(),
                 result.networks[idx].channel,
                 (int)rssi);

        // Truncate display if too long
        String display = String(line);
        if (display.length() > 38) {
            display = display.substring(0, 35) + "...";
        }
        _sprite.drawString(display.c_str(), 4, y + 2);
    }

    if (result.count == 0) {
        _sprite.setTextColor(TFT_RED);
        _sprite.setTextDatum(MC_DATUM);
        _sprite.drawString("No networks found", SCREEN_W / 2, SCREEN_H / 2);
    }

    _sprite.pushSprite(0, 0);
}

void Display::drawMonitor(const WifiScanner& scanner, bool buzzerOn) {
    ensureSprite();
    _sprite.fillSprite(BLACK);
    _sprite.setTextDatum(TL_DATUM);

    int32_t rssi = scanner.getLockedRSSI();

    // Row 1: SSID + channel (top bar)
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_CYAN);
    char topBar[48];
    snprintf(topBar, sizeof(topBar), "%s  CH:%d  %s",
             scanner.lockedSSID().c_str(),
             scanner.lockedChannel(),
             buzzerOn ? "[BZR]" : "");
    _sprite.drawString(topBar, 2, 2);

    drawBattery(SCREEN_W - 28, 2);

    // Row 2: Big dBm number
    _sprite.setTextSize(4);
    _sprite.setTextColor(rssiColor(rssi));
    char dbmStr[16];
    snprintf(dbmStr, sizeof(dbmStr), "%d", (int)rssi);
    _sprite.setTextDatum(TL_DATUM);
    _sprite.drawString(dbmStr, 4, 16);

    // dBm label next to number
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_DARKGREY);
    _sprite.drawString("dBm", 4 + strlen(dbmStr) * 24 + 4, 20);

    // Quality label
    _sprite.setTextSize(2);
    _sprite.setTextColor(rssiColor(rssi));
    _sprite.setTextDatum(TR_DATUM);
    _sprite.drawString(rssiLabel(rssi), SCREEN_W - 4, 16);

    // Min / Max stats
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_DARKGREY);
    _sprite.setTextDatum(TR_DATUM);
    char stats[32];
    snprintf(stats, sizeof(stats), "Min:%d Max:%d",
             (int)scanner.getMinRSSI(), (int)scanner.getMaxRSSI());
    _sprite.drawString(stats, SCREEN_W - 4, 36);

    // Signal bar
    drawSignalBar(4, 50, SCREEN_W - 8, 10, rssi);

    // History graph (bottom) — fills remaining space
    drawHistoryGraph(4, 64, SCREEN_W - 8, SCREEN_H - 68, scanner);

    // Button hints at bottom
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_DARKGREY);
    _sprite.setTextDatum(BL_DATUM);
    _sprite.drawString("A:back", 2, SCREEN_H - 1);
    _sprite.setTextDatum(BR_DATUM);
    _sprite.drawString("B:buzzer", SCREEN_W - 2, SCREEN_H - 1);

    _sprite.pushSprite(0, 0);
}

uint16_t Display::rssiColor(int32_t rssi) {
    if (rssi > -50) return TFT_GREEN;
    if (rssi > -60) return 0x07E0; // bright green
    if (rssi > -70) return TFT_YELLOW;
    if (rssi > -80) return TFT_ORANGE;
    return TFT_RED;
}

const char* Display::rssiLabel(int32_t rssi) {
    if (rssi > -50) return "GREAT";
    if (rssi > -60) return "GOOD";
    if (rssi > -70) return "FAIR";
    if (rssi > -80) return "WEAK";
    return "POOR";
}

void Display::drawSignalBar(int x, int y, int w, int h, int32_t rssi) {
    // Map RSSI -100..-30 to 0..w
    int fillW = map(constrain(rssi, -100, -30), -100, -30, 0, w);

    _sprite.drawRect(x, y, w, h, TFT_DARKGREY);
    if (fillW > 0) {
        _sprite.fillRect(x + 1, y + 1, fillW - 2, h - 2, rssiColor(rssi));
    }
}

void Display::drawHistoryGraph(int x, int y, int w, int h, const WifiScanner& scanner) {
    _sprite.drawRect(x, y, w, h, TFT_DARKGREY);

    const int32_t* history = scanner.getRSSIHistory();
    int histLen = scanner.getRSSIHistoryLen();
    int headIdx = scanner.getRSSIHistoryIndex();

    // Draw gridlines at -50, -70, -90
    _sprite.setTextSize(1);
    for (int db : {-50, -70, -90}) {
        int gy = y + map(constrain(db, -100, -30), -30, -100, 0, h - 1);
        _sprite.drawFastHLine(x, gy, w, 0x2104); // dark grey
    }

    // Plot points right-to-left from head
    int plotW = min(w - 2, histLen);
    int prevPy = -1;

    for (int i = 0; i < plotW; i++) {
        int dataIdx = (headIdx - 1 - i + histLen) % histLen;
        int32_t val = history[dataIdx];
        if (val == 0) continue;

        int px = x + w - 2 - i;
        int py = y + 1 + map(constrain(val, -100, -30), -30, -100, 0, h - 3);

        _sprite.drawPixel(px, py, rssiColor(val));
        if (prevPy >= 0 && abs(py - prevPy) <= 3) {
            _sprite.drawLine(px + 1, prevPy, px, py, rssiColor(val));
        }
        prevPy = py;
    }
}

void Display::drawBattery(int x, int y) {
    float volts = M5.Axp.GetBatVoltage();
    int pct = constrain((int)((volts - 3.0f) / (4.2f - 3.0f) * 100.0f), 0, 100);

    uint16_t color;
    if (pct > 50) color = TFT_GREEN;
    else if (pct > 20) color = TFT_YELLOW;
    else color = TFT_RED;

    // Battery icon: 24x8 outline + 2px nub on right
    _sprite.drawRect(x, y, 22, 8, TFT_DARKGREY);
    _sprite.fillRect(x + 22, y + 2, 2, 4, TFT_DARKGREY);

    int fillW = (pct * 20) / 100;
    if (fillW > 0) {
        _sprite.fillRect(x + 1, y + 1, fillW, 6, color);
    }
}
