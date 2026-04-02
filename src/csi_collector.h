#pragma once
#include <Arduino.h>
#include "esp_wifi.h"

// 64 raw subcarriers; active = indices 0-24 and 36-61 (25+26 = 51)
// Indices 25-35 are DC/guard nulls, 62-63 are garbage
static constexpr int CSI_RAW_SUB = 64;
static constexpr int CSI_ACTIVE_LEFT = 25;
static constexpr int CSI_ACTIVE_RIGHT = 26;
static constexpr int CSI_ACTIVE_SUB = 51;
static constexpr int CSI_WATERFALL_ROWS = 120;

class CSICollector {
public:
    // Connected mode: join AP and generate traffic for high CSI rates
    void beginConnected(const char* ssid, const char* password, uint8_t channel);
    // Promiscuous mode: passive sniffing, no password needed
    void beginPassive(uint8_t channel, const uint8_t* targetBSSID = nullptr);
    void stop();

    bool isConnected() const { return _connected; }

    // Waterfall ring buffer
    uint8_t waterfall[CSI_WATERFALL_ROWS][CSI_ACTIVE_SUB] = {};
    volatile int head = 0;
    volatile int rowCount = 0;

    // Smoothed amplitudes for equalizer view (0.0 - 1.0)
    float amplitudes[CSI_ACTIVE_SUB] = {};
    float peaks[CSI_ACTIVE_SUB] = {};

    int activeCount = CSI_ACTIVE_SUB;
    volatile bool hasData = false;
    volatile int32_t latestRSSI = -100;

    // Packet rate tracking
    volatile int pktCount = 0;
    int pktPerSec = 0;
    void updateRate();

    void decayPeaks();

private:
    void enableCSI();
    void resetBuffers();

    static void IRAM_ATTR _cb(void* ctx, wifi_csi_info_t* info);
    static void _trafficTask(void* param);

    float _runningMax = 1.0f;
    volatile bool _active = false;
    bool _connected = false;
    uint8_t _targetBSSID[6] = {};
    bool _filterBSSID = false;

    TaskHandle_t _trafficTaskHandle = nullptr;
};
