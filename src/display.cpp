#include "display.h"

void Display::begin() {
    M5.Lcd.setRotation(1);
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

// Heatmap: black → blue → cyan → green → yellow → red → white
static uint16_t heatColor(uint8_t v) {
    uint8_t r, g, b;
    if (v < 51) {           // black → blue
        r = 0; g = 0; b = v * 5;
    } else if (v < 102) {   // blue → cyan
        uint8_t t = (v - 51) * 5;
        r = 0; g = t; b = 255;
    } else if (v < 153) {   // cyan → green
        uint8_t t = (v - 102) * 5;
        r = 0; g = 255; b = 255 - t;
    } else if (v < 204) {   // green → yellow
        uint8_t t = (v - 153) * 5;
        r = t; g = 255; b = 0;
    } else {                 // yellow → red
        uint8_t t = (v - 204) * 5;
        r = 255; g = 255 - t; b = 0;
    }
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void Display::drawCSIFooter(int32_t rssi, bool buzzerOn, int pktPerSec) {
    _sprite.setTextSize(1);
    _sprite.setTextColor(rssiColor(rssi));
    char rssiStr[24];
    snprintf(rssiStr, sizeof(rssiStr), "%ddBm %dpkt/s", (int)rssi, pktPerSec);
    _sprite.setTextDatum(BL_DATUM);
    _sprite.drawString(rssiStr, 2, SCREEN_H - 1);

    _sprite.setTextColor(TFT_DARKGREY);
    _sprite.setTextDatum(BR_DATUM);
    _sprite.drawString("B:bzr PWR:view", SCREEN_W - 2, SCREEN_H - 1);
}

void Display::drawCSIWaterfall(const String& ssid, int channel, const CSICollector& csi, bool buzzerOn) {
    ensureSprite();
    _sprite.fillSprite(BLACK);
    _sprite.setTextDatum(TL_DATUM);
    _sprite.setTextSize(1);

    // Header
    _sprite.setTextColor(TFT_CYAN);
    char topBar[56];
    snprintf(topBar, sizeof(topBar), "%s CH:%d %s%s",
             ssid.c_str(), channel,
             csi.isConnected() ? "[CONN] " : "[PASV] ",
             buzzerOn ? "[BZR]" : "");
    _sprite.drawString(topBar, 2, 2);
    drawBattery(SCREEN_W - 28, 2);

    // Waterfall area — two halves (lower/upper subcarriers) with DC divider
    static constexpr int TOP_Y = 13;
    static constexpr int BOT_Y = SCREEN_H - 11;
    static constexpr int DC_GAP = 1;
    int wfHeight = BOT_Y - TOP_Y;
    int numLeft = CSI_ACTIVE_LEFT;
    int numRight = CSI_ACTIVE_RIGHT;
    int availW = SCREEN_W - DC_GAP;
    int cellW = availW / (numLeft + numRight);
    if (cellW < 1) cellW = 1;
    int leftW = numLeft * cellW;
    int rightW = numRight * cellW;
    int totalW = leftW + DC_GAP + rightW;
    int startX = (SCREEN_W - totalW) / 2;
    int dcX = startX + leftW;

    // Draw waterfall: newest row at top, oldest at bottom
    int rows = min((int)csi.rowCount, wfHeight);
    for (int r = 0; r < rows; r++) {
        int bufIdx = ((int)csi.head - 1 - r + CSI_WATERFALL_ROWS) % CSI_WATERFALL_ROWS;
        int y = TOP_Y + r;
        const uint8_t* row = csi.waterfall[bufIdx];

        for (int s = 0; s < numLeft; s++) {
            _sprite.fillRect(startX + s * cellW, y, cellW, 1, heatColor(row[s]));
        }
        for (int s = 0; s < numRight; s++) {
            _sprite.fillRect(dcX + DC_GAP + s * cellW, y, cellW, 1, heatColor(row[numLeft + s]));
        }
    }

    // DC divider line
    _sprite.drawFastVLine(dcX, TOP_Y, wfHeight, 0x4208);

    drawCSIFooter(csi.latestRSSI, buzzerOn, csi.pktPerSec);
    _sprite.pushSprite(0, 0);
}

void Display::drawCSIEqualizer(const String& ssid, int channel, const CSICollector& csi, bool buzzerOn) {
    ensureSprite();
    _sprite.fillSprite(BLACK);
    _sprite.setTextDatum(TL_DATUM);
    _sprite.setTextSize(1);

    // Header
    _sprite.setTextColor(TFT_CYAN);
    char topBar[56];
    snprintf(topBar, sizeof(topBar), "%s CH:%d %s%s",
             ssid.c_str(), channel,
             csi.isConnected() ? "[CONN] " : "[PASV] ",
             buzzerOn ? "[BZR]" : "");
    _sprite.drawString(topBar, 2, 2);
    drawBattery(SCREEN_W - 28, 2);

    // Equalizer area
    static constexpr int TOP_Y = 14;
    static constexpr int BOT_Y = SCREEN_H - 12;
    static constexpr int BAR_MAX_H = BOT_Y - TOP_Y;
    int numBars = csi.activeCount > 0 ? csi.activeCount : CSI_ACTIVE_SUB;
    int barW = SCREEN_W / numBars;
    if (barW < 2) barW = 2;
    int totalW = numBars * barW;
    int startX = (SCREEN_W - totalW) / 2;

    for (int i = 0; i < numBars; i++) {
        float amp = csi.amplitudes[i];
        if (amp < 0.0f) amp = 0.0f;
        if (amp > 1.0f) amp = 1.0f;

        int barH = (int)(amp * BAR_MAX_H);
        if (barH < 1 && amp > 0.01f) barH = 1;

        int x = startX + i * barW;
        int bw = barW > 1 ? barW - 1 : 1;

        if (barH > 0) {
            int greenH = min(barH, (int)(BAR_MAX_H * 0.4f));
            int yellowH = min(barH, (int)(BAR_MAX_H * 0.7f)) - greenH;
            int redH = barH - greenH - (yellowH > 0 ? yellowH : 0);
            if (yellowH < 0) yellowH = 0;
            if (redH < 0) redH = 0;

            int y = BOT_Y;
            if (greenH > 0) {
                _sprite.fillRect(x, y - greenH, bw, greenH, TFT_GREEN);
                y -= greenH;
            }
            if (yellowH > 0) {
                _sprite.fillRect(x, y - yellowH, bw, yellowH, TFT_YELLOW);
                y -= yellowH;
            }
            if (redH > 0) {
                _sprite.fillRect(x, y - redH, bw, redH, TFT_RED);
            }
        }

        // Peak hold indicator
        float peak = csi.peaks[i];
        if (peak > 1.0f) peak = 1.0f;
        int peakY = BOT_Y - (int)(peak * BAR_MAX_H);
        if (peak > 0.02f) {
            _sprite.drawFastHLine(x, peakY, barW > 1 ? barW - 1 : 1, TFT_WHITE);
        }
    }

    // DC divider
    int dcBarX = startX + CSI_ACTIVE_LEFT * barW;
    _sprite.drawFastVLine(dcBarX - 1, TOP_Y, BAR_MAX_H, 0x4208);

    drawCSIFooter(csi.latestRSSI, buzzerOn, csi.pktPerSec);
    _sprite.pushSprite(0, 0);
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
