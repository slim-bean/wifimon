#include <M5StickCPlus.h>
#include "wifi_scanner.h"
#include "display.h"
#include "audio.h"

static WifiScanner scanner;
static Display display;
static Audio audio;

static AppScreen currentScreen = AppScreen::NETWORK_LIST;
static ScanResult lastScan;
static int selectedNetworkIndex = 0;
static bool initialScanDone = false;
static unsigned long lastScanStartMs = 0;

static constexpr unsigned long SCAN_INTERVAL_LIST_MS = 5000;
static constexpr unsigned long FULL_SCAN_INTERVAL_MS = 10000;
static unsigned long lastFullScanMs = 0;

void startScan() {
    if (currentScreen == AppScreen::MONITOR && scanner.isLocked() && scanner.lockedChannel() > 0) {
        scanner.startFastChannelScan(scanner.lockedChannel());
    } else {
        scanner.startAsyncScan();
    }
    lastScanStartMs = millis();
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
            scanner.lockNetwork(net.ssid, net.channel);
            currentScreen = AppScreen::MONITOR;
            startScan();
        }
    }
}

void handleMonitorInput() {
    if (M5.BtnA.wasPressed()) {
        scanner.unlockNetwork();
        currentScreen = AppScreen::NETWORK_LIST;
        selectedNetworkIndex = 0;
        startScan();
    }

    if (M5.BtnB.wasPressed()) {
        audio.toggle();
    }
}

void setup() {
    M5.begin();
    M5.Lcd.setRotation(3);

    display.begin();
    display.drawSplash();

    scanner.begin();
    audio.begin();

    startScan();
}

void loop() {
    M5.update();

    bool scanDone = scanner.isScanComplete();

    if (scanDone) {
        lastScan = scanner.getResults();
        initialScanDone = true;

        if (currentScreen == AppScreen::MONITOR && scanner.isLocked()) {
            scanner.updateFromScan(lastScan);
        }

        if (currentScreen == AppScreen::MONITOR) {
            startScan();
        } else if (millis() - lastScanStartMs >= SCAN_INTERVAL_LIST_MS) {
            startScan();
        }
    }

    switch (currentScreen) {
        case AppScreen::NETWORK_LIST:
            handleNetworkListInput();
            if (initialScanDone) {
                display.drawNetworkList(lastScan, selectedNetworkIndex);
            }
            break;

        case AppScreen::MONITOR:
            handleMonitorInput();
            if (scanner.isLocked()) {
                audio.update(scanner.getLockedRSSI());
                display.drawMonitor(scanner, audio.isEnabled());
            }
            break;
    }
}
