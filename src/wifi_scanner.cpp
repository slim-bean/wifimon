#include "wifi_scanner.h"

void WifiScanner::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
}

void WifiScanner::startAsyncScan() {
    WiFi.scanNetworks(true, false, false, 100);
}

void WifiScanner::startFastChannelScan(uint8_t channel) {
    WiFi.scanDelete();

    wifi_scan_config_t config = {};
    config.ssid = NULL;
    config.bssid = NULL;
    config.channel = channel;
    config.show_hidden = false;
    config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    config.scan_time.active.min = 10;
    config.scan_time.active.max = 30;

    esp_wifi_scan_start(&config, false);
}

bool WifiScanner::isScanComplete() {
    return WiFi.scanComplete() >= 0;
}

ScanResult WifiScanner::getResults() {
    ScanResult result;
    int n = WiFi.scanComplete();
    if (n < 0) {
        result.count = 0;
        return result;
    }
    result.count = min(n, MAX_NETWORKS);
    for (int i = 0; i < result.count; i++) {
        result.networks[i].ssid = WiFi.SSID(i);
        result.networks[i].rssi = WiFi.RSSI(i);
        result.networks[i].channel = WiFi.channel(i);
        result.networks[i].encryption = WiFi.encryptionType(i);
        uint8_t* b = WiFi.BSSID(i);
        if (b) memcpy(result.networks[i].bssid, b, 6);
        else memset(result.networks[i].bssid, 0, 6);
    }
    WiFi.scanDelete();
    return result;
}

void WifiScanner::lockNetwork(const String& ssid, uint8_t channel, const uint8_t* bssid) {
    _locked = true;
    _lockedSSID = ssid;
    _lockedChannel = channel;
    if (bssid) memcpy(_lockedBSSID, bssid, 6);
    else memset(_lockedBSSID, 0, 6);
    resetStats();
}

void WifiScanner::unlockNetwork() {
    _locked = false;
    _lockedSSID = "";
    _lockedChannel = 0;
    resetStats();
}

void WifiScanner::resetStats() {
    _minRSSI = 0;
    _maxRSSI = -100;
    _rawRSSI = -100;
    _smoothedRSSI = -100;
    _historyIndex = 0;
    _historyFilled = false;
    _smoothIndex = 0;
    memset(_rssiHistory, 0, sizeof(_rssiHistory));
    memset(_smoothBuffer, 0, sizeof(_smoothBuffer));
}

void WifiScanner::updateFromScan(const ScanResult& result) {
    if (!_locked) return;

    int32_t foundRSSI = -120;
    bool found = false;
    int congestionCount = 0;
    int32_t strongestInterferer = -120;

    for (int i = 0; i < result.count; i++) {
        if (result.networks[i].ssid == _lockedSSID) {
            foundRSSI = result.networks[i].rssi;
            _lockedChannel = result.networks[i].channel;
            found = true;
        }

        if (_lockedChannel > 0) {
            int ch = result.networks[i].channel;
            bool overlaps = abs(ch - _lockedChannel) <= 2;
            if (overlaps && result.networks[i].ssid != _lockedSSID) {
                congestionCount++;
                if (result.networks[i].rssi > strongestInterferer) {
                    strongestInterferer = result.networks[i].rssi;
                }
            }
        }
    }

    _congestion.networkCount = congestionCount;
    _congestion.strongestInterferer = strongestInterferer;

    if (found) {
        _rawRSSI = foundRSSI;
        _smoothedRSSI = computeSmoothed(foundRSSI);
        pushRSSI(_smoothedRSSI);

        if (_smoothedRSSI < _minRSSI || _minRSSI == 0) _minRSSI = _smoothedRSSI;
        if (_smoothedRSSI > _maxRSSI) _maxRSSI = _smoothedRSSI;
    }
}

void WifiScanner::pushRSSI(int32_t rssi) {
    _rssiHistory[_historyIndex] = rssi;
    _historyIndex = (_historyIndex + 1) % RSSI_HISTORY_LEN;
    if (_historyIndex == 0) _historyFilled = true;
}

int32_t WifiScanner::computeSmoothed(int32_t rssi) {
    _smoothBuffer[_smoothIndex] = rssi;
    _smoothIndex = (_smoothIndex + 1) % RSSI_SMOOTHING_WINDOW;

    int32_t sum = 0;
    int count = 0;
    for (int i = 0; i < RSSI_SMOOTHING_WINDOW; i++) {
        if (_smoothBuffer[i] != 0) {
            sum += _smoothBuffer[i];
            count++;
        }
    }
    return count > 0 ? sum / count : rssi;
}
