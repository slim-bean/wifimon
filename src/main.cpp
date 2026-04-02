#include <M5StickCPlus.h>
#include "wifi_scanner.h"
#include "display.h"
#include "audio.h"
#include "csi_collector.h"
#include "credentials.h"

// Look up password for an SSID, returns nullptr if not found
static const char* findPassword(const String& ssid) {
    for (int i = 0; i < WIFI_CREDENTIALS_COUNT; i++) {
        if (ssid == WIFI_CREDENTIALS[i].ssid) {
            return WIFI_CREDENTIALS[i].password;
        }
    }
    return nullptr;
}

static WifiScanner scanner;
static Display display;
static Audio audio;
static CSICollector csi;

static AppScreen currentScreen = AppScreen::NETWORK_LIST;
static ScanResult lastScan;
static int selectedNetworkIndex = 0;
static bool initialScanDone = false;
static unsigned long lastScanStartMs = 0;
static unsigned long lastPeakDecayMs = 0;
static bool csiRunning = false;

static constexpr unsigned long SCAN_INTERVAL_LIST_MS = 5000;
static constexpr unsigned long PEAK_DECAY_INTERVAL_MS = 50;
static bool pwrPressed = false;

void startScan() {
    if (currentScreen == AppScreen::MONITOR && scanner.isLocked() && scanner.lockedChannel() > 0) {
        scanner.startFastChannelScan(scanner.lockedChannel());
    } else {
        scanner.startAsyncScan();
    }
    lastScanStartMs = millis();
}

void enterCSIMode() {
    if (!csiRunning) {
        const char* pass = findPassword(scanner.lockedSSID());
        if (pass) {
            csi.beginConnected(scanner.lockedSSID().c_str(), pass, scanner.lockedChannel());
        } else {
            csi.beginPassive(scanner.lockedChannel(), scanner.lockedBSSID());
        }
        csiRunning = true;
    }
}

void exitCSIMode() {
    if (csiRunning) {
        bool wasConnected = csi.isConnected();
        csi.stop();
        csiRunning = false;
        // After connected mode, re-init scanner for scanning
        if (wasConnected) {
            scanner.begin();
        }
    }
}

void handleNetworkListInput() {
    if (M5.BtnB.wasPressed()) {
        if (lastScan.count > 0) {
            selectedNetworkIndex = (selectedNetworkIndex + 1) % lastScan.count;
        }
    }

    if (M5.BtnA.wasPressed()) {
        if (lastScan.count > 0 && selectedNetworkIndex < lastScan.count) {
            auto& net = lastScan.networks[selectedNetworkIndex];
            scanner.lockNetwork(net.ssid, net.channel, net.bssid);
            currentScreen = AppScreen::MONITOR;
            startScan();
        }
    }
}

void handleLockedInput() {
    // A: back to network list (from any view)
    if (M5.BtnA.wasPressed()) {
        exitCSIMode();
        scanner.unlockNetwork();
        currentScreen = AppScreen::NETWORK_LIST;
        selectedNetworkIndex = 0;
        startScan();
        return;
    }

    // B: toggle buzzer
    if (M5.BtnB.wasPressed()) {
        audio.toggle();
    }

    // PWR press: cycle views (accept short or long press)
    if (pwrPressed) {
        switch (currentScreen) {
            case AppScreen::MONITOR:
                enterCSIMode();
                currentScreen = AppScreen::CSI_WATERFALL;
                break;
            case AppScreen::CSI_WATERFALL:
                currentScreen = AppScreen::CSI_EQUALIZER;
                break;
            case AppScreen::CSI_EQUALIZER:
                exitCSIMode();
                currentScreen = AppScreen::MONITOR;
                startScan();
                break;
            default:
                break;
        }
    }
}

void setup() {
    M5.begin();
    M5.Lcd.setRotation(1);

    display.begin();
    display.drawSplash();

    scanner.begin();
    audio.begin();

    startScan();
}

void loop() {
    M5.update();
    pwrPressed = (M5.Axp.GetBtnPress() != 0);

    bool scanDone = scanner.isScanComplete();

    if (scanDone) {
        lastScan = scanner.getResults();
        initialScanDone = true;

        if (currentScreen == AppScreen::MONITOR && scanner.isLocked()) {
            scanner.updateFromScan(lastScan);
        }

        if (currentScreen == AppScreen::MONITOR) {
            startScan();
        }
    }

    // In network list, rescan on interval
    if (currentScreen == AppScreen::NETWORK_LIST &&
        millis() - lastScanStartMs >= SCAN_INTERVAL_LIST_MS) {
        startScan();
    }

    switch (currentScreen) {
        case AppScreen::NETWORK_LIST:
            handleNetworkListInput();
            if (initialScanDone) {
                display.drawNetworkList(lastScan, selectedNetworkIndex);
            }
            break;

        case AppScreen::MONITOR:
            handleLockedInput();
            if (scanner.isLocked()) {
                audio.update(scanner.getLockedRSSI());
                display.drawMonitor(scanner, audio.isEnabled());
            }
            break;

        case AppScreen::CSI_WATERFALL:
            handleLockedInput();
            csi.updateRate();
            if (csi.hasData) {
                audio.update(csi.latestRSSI);
            }
            display.drawCSIWaterfall(scanner.lockedSSID(), scanner.lockedChannel(), csi, audio.isEnabled());
            break;

        case AppScreen::CSI_EQUALIZER:
            handleLockedInput();
            csi.updateRate();
            if (csi.hasData) {
                audio.update(csi.latestRSSI);
            }
            if (millis() - lastPeakDecayMs >= PEAK_DECAY_INTERVAL_MS) {
                csi.decayPeaks();
                lastPeakDecayMs = millis();
            }
            display.drawCSIEqualizer(scanner.lockedSSID(), scanner.lockedChannel(), csi, audio.isEnabled());
            break;
    }
}
