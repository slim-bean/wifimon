#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

static constexpr int MAX_NETWORKS = 32;
static constexpr int RSSI_HISTORY_LEN = 232;
static constexpr int RSSI_SMOOTHING_WINDOW = 5;

struct NetworkInfo {
    String ssid;
    int32_t rssi;
    uint8_t channel;
    wifi_auth_mode_t encryption;
};

struct ChannelCongestion {
    int networkCount;
    int32_t strongestInterferer;
};

struct ScanResult {
    NetworkInfo networks[MAX_NETWORKS];
    int count;
};

class WifiScanner {
public:
    void begin();
    void startAsyncScan();
    void startFastChannelScan(uint8_t channel);
    bool isScanComplete();
    ScanResult getResults();

    void lockNetwork(const String& ssid, uint8_t channel);
    void unlockNetwork();
    bool isLocked() const { return _locked; }
    const String& lockedSSID() const { return _lockedSSID; }
    int lockedChannel() const { return _lockedChannel; }

    int32_t getLockedRSSI() const { return _smoothedRSSI; }
    int32_t getLockedRSSIRaw() const { return _rawRSSI; }
    int32_t getMinRSSI() const { return _minRSSI; }
    int32_t getMaxRSSI() const { return _maxRSSI; }

    const int32_t* getRSSIHistory() const { return _rssiHistory; }
    int getRSSIHistoryLen() const { return RSSI_HISTORY_LEN; }
    int getRSSIHistoryIndex() const { return _historyIndex; }

    ChannelCongestion getChannelCongestion() const { return _congestion; }

    void updateFromScan(const ScanResult& result);
    void resetStats();

private:
    bool _locked = false;
    String _lockedSSID;
    uint8_t _lockedChannel = 0;

    int32_t _rawRSSI = -100;
    int32_t _smoothedRSSI = -100;
    int32_t _minRSSI = 0;
    int32_t _maxRSSI = -100;

    int32_t _rssiHistory[RSSI_HISTORY_LEN] = {};
    int _historyIndex = 0;
    bool _historyFilled = false;

    int32_t _smoothBuffer[RSSI_SMOOTHING_WINDOW] = {};
    int _smoothIndex = 0;

    ChannelCongestion _congestion = {0, -100};

    void pushRSSI(int32_t rssi);
    int32_t computeSmoothed(int32_t rssi);
};
