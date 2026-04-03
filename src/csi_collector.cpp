#include "csi_collector.h"
#include <WiFi.h>
#include "lwip/inet_chksum.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"

static void IRAM_ATTR _promiscRxCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    // No-op — needed so the WiFi stack delivers packets to CSI
}

void CSICollector::resetBuffers() {
    hasData = false;
    latestRSSI = -100;
    _runningMax = 1.0f;
    head = 0;
    rowCount = 0;
    pktCount = 0;
    pktPerSec = 0;
    memset(waterfall, 0, sizeof(waterfall));
    memset(amplitudes, 0, sizeof(amplitudes));
    memset(peaks, 0, sizeof(peaks));
}

void CSICollector::enableCSI() {
    wifi_csi_config_t cfg = {};
    cfg.lltf_en = true;
    cfg.htltf_en = true;
    cfg.stbc_htltf2_en = false;
    cfg.ltf_merge_en = true;
    cfg.channel_filter_en = false;
    cfg.manu_scale = false;
    cfg.shift = 0;

    esp_wifi_set_csi_config(&cfg);
    esp_wifi_set_csi_rx_cb(_cb, this);
    esp_wifi_set_csi(true);
}

// --- Traffic generator task: pings gateway to stimulate CSI ---

// Raw ICMP receive callback (we don't need the replies, just discard)
static uint8_t _pingRecvCb(void* arg, struct raw_pcb* pcb, struct pbuf* p, const ip_addr_t* addr) {
    pbuf_free(p);
    return 1;  // consumed
}

void CSICollector::_trafficTask(void* param) {
    CSICollector* self = static_cast<CSICollector*>(param);

    // Wait for connection
    while (self->_active && WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    IPAddress gw = WiFi.gatewayIP();
    Serial.printf("CSI ping gen: gateway=%s\n", gw.toString().c_str());

    ip_addr_t target;
    target.type = IPADDR_TYPE_V4;
    target.u_addr.ip4.addr = (uint32_t)gw;

    // Create raw ICMP socket
    struct raw_pcb* pcb = raw_new(IP_PROTO_ICMP);
    if (!pcb) {
        Serial.println("CSI: failed to create ICMP pcb");
        vTaskDelete(nullptr);
        return;
    }
    raw_recv(pcb, _pingRecvCb, nullptr);

    uint16_t seq = 0;

    while (self->_active) {
        if (WiFi.status() == WL_CONNECTED) {
            // Build minimal ICMP echo request (8 byte header, no payload)
            struct pbuf* p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr), PBUF_RAM);
            if (p) {
                struct icmp_echo_hdr* hdr = (struct icmp_echo_hdr*)p->payload;
                ICMPH_TYPE_SET(hdr, ICMP_ECHO);
                ICMPH_CODE_SET(hdr, 0);
                hdr->id = htons(0xC5DA);
                hdr->seqno = htons(seq++);
                hdr->chksum = 0;
                hdr->chksum = inet_chksum(hdr, sizeof(struct icmp_echo_hdr));

                raw_sendto(pcb, p, &target);
                pbuf_free(p);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // ~50 pings/sec
    }

    raw_remove(pcb);
    vTaskDelete(nullptr);
}

// --- Connected mode: join AP and generate traffic ---

void CSICollector::beginConnected(const char* ssid, const char* password, uint8_t channel) {
    _active = true;
    _connected = false;
    _filterBSSID = false;  // no MAC filter needed when connected
    resetBuffers();

    // Stop any in-progress scan
    WiFi.scanDelete();
    esp_wifi_scan_stop();

    // Disable promiscuous mode — connected mode doesn't need it
    esp_wifi_set_promiscuous(false);

    // Connect to the AP
    WiFi.begin(ssid, password);
    Serial.printf("CSI: connecting to %s...\n", ssid);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        _connected = true;
        Serial.printf("CSI: connected, IP=%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("CSI: connection failed, falling back to passive");
        WiFi.disconnect();
        beginPassive(channel);
        return;
    }

    // Enable CSI — when connected, CSI is generated for all frames from the AP
    enableCSI();

    // Start traffic generator
    xTaskCreatePinnedToCore(_trafficTask, "csi_traffic", 4096, this, 1,
                            &_trafficTaskHandle, 0);
}

// --- Passive mode: promiscuous sniffing ---

void CSICollector::beginPassive(uint8_t channel, const uint8_t* targetBSSID) {
    _active = true;
    _connected = false;
    resetBuffers();

    if (targetBSSID) {
        memcpy(_targetBSSID, targetBSSID, 6);
        _filterBSSID = true;
    } else {
        _filterBSSID = false;
    }

    // Stop any in-progress scan
    WiFi.scanDelete();
    esp_wifi_scan_stop();

    // Enable promiscuous mode
    esp_wifi_set_promiscuous_rx_cb(_promiscRxCb);
    esp_wifi_set_promiscuous(true);

    // Set channel
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    enableCSI();
}

void CSICollector::stop() {
    _active = false;

    // Wait for traffic task to finish
    if (_trafficTaskHandle) {
        vTaskDelay(pdMS_TO_TICKS(150));
        _trafficTaskHandle = nullptr;
    }

    esp_wifi_set_csi(false);
    esp_wifi_set_csi_rx_cb(nullptr, nullptr);

    if (_connected) {
        WiFi.disconnect();
        _connected = false;
        // Re-init STA mode for scanning
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(50);
    } else {
        esp_wifi_set_promiscuous(false);
    }

    hasData = false;
}

void IRAM_ATTR CSICollector::_cb(void* ctx, wifi_csi_info_t* info) {
    CSICollector* self = static_cast<CSICollector*>(ctx);
    if (!self || !info || !info->buf) return;

    // In passive mode, filter by last 5 bytes of MAC
    if (self->_filterBSSID &&
        memcmp(info->mac + 1, self->_targetBSSID + 1, 5) != 0) {
        return;
    }

    self->latestRSSI = info->rx_ctrl.rssi;

    int offset = info->first_word_invalid ? 4 : 0;
    int usableBytes = info->len - offset;
    if (usableBytes < 4) return;

    int numRaw = min(usableBytes / 2, (int)CSI_RAW_SUB);
    int8_t* buf = info->buf + offset;

    // Compute raw amplitude for all subcarriers
    float rawAmps[CSI_RAW_SUB] = {};
    for (int i = 0; i < numRaw; i++) {
        float I = buf[i * 2];
        float Q = buf[i * 2 + 1];
        rawAmps[i] = sqrtf(I * I + Q * Q);
    }

    // Extract active subcarriers (skip DC/guard nulls and trailing garbage)
    float active[CSI_ACTIVE_SUB];
    int out = 0;

    int loEnd = min(25, numRaw);
    int hiStart = min(36, numRaw);
    int hiEnd = min(62, numRaw);

    for (int i = 0; i < loEnd && out < CSI_ACTIVE_SUB; i++) {
        active[out++] = rawAmps[i];
    }
    for (int i = hiStart; i < hiEnd && out < CSI_ACTIVE_SUB; i++) {
        active[out++] = rawAmps[i];
    }

    self->activeCount = out;

    // Find max for normalization
    float frameMax = 0;
    for (int i = 0; i < out; i++) {
        if (active[i] > frameMax) frameMax = active[i];
    }

    // Update running max with slow decay
    if (frameMax > self->_runningMax) {
        self->_runningMax = frameMax;
    } else {
        self->_runningMax = self->_runningMax * 0.998f + frameMax * 0.002f;
    }
    if (self->_runningMax < 1.0f) self->_runningMax = 1.0f;

    float invMax = 1.0f / self->_runningMax;

    // Write normalized row (0-255) into waterfall ring buffer
    int row = self->head;
    for (int i = 0; i < out; i++) {
        float n = active[i] * invMax;
        if (n > 1.0f) n = 1.0f;
        self->waterfall[row][i] = (uint8_t)(n * 255.0f);

        // Smoothed amplitudes for equalizer (0.0 - 1.0)
        self->amplitudes[i] = self->amplitudes[i] * 0.3f + n * 0.7f;
        if (self->amplitudes[i] > self->peaks[i]) {
            self->peaks[i] = self->amplitudes[i];
        }
    }
    for (int i = out; i < CSI_ACTIVE_SUB; i++) {
        self->waterfall[row][i] = 0;
    }

    self->head = (row + 1) % CSI_WATERFALL_ROWS;
    if (self->rowCount < CSI_WATERFALL_ROWS) self->rowCount++;
    self->pktCount++;
    self->hasData = true;
}

void CSICollector::updateRate() {
    static unsigned long lastRateMs = 0;
    unsigned long now = millis();
    if (now - lastRateMs >= 1000) {
        pktPerSec = pktCount;
        pktCount = 0;
        lastRateMs = now;
    }
}

void CSICollector::decayPeaks() {
    for (int i = 0; i < activeCount; i++) {
        peaks[i] *= 0.96f;
        if (peaks[i] < amplitudes[i]) peaks[i] = amplitudes[i];
    }
}
