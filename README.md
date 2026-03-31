# WiFiMon

ESP32 M5StickC Plus 1.1 based realtime WiFi signal strength monitor with channel congestion display.

Designed to help find optimal placement for WiFi repeaters by walking around and observing signal strength in real time.

## Features

- **Real-time RSSI monitoring** — scans continuously as fast as the ESP32 allows
- **Color-coded signal bar** and large dBm readout with quality label
- **Rolling signal history graph** — see trends as you walk
- **Channel congestion indicator** — shows how many networks overlap your target's channel
- **Geiger counter mode** — buzzer beeps faster and higher-pitched as signal strengthens (toggle with Button B)
- **Network selection** — scan and pick your target SSID on boot
- **Moving average smoothing** for stable, readable values
- **Min/Max tracking** across your session

## Hardware

- M5StickC Plus 1.1

## Controls

| Button | Network List | Monitor |
|--------|-------------|---------|
| A (front) | Select network | Back to list |
| B (side) | Next network | Toggle buzzer |

## Signal Strength Reference

| RSSI | Quality | Repeater Suitability |
|------|---------|---------------------|
| > -50 dBm | Great | Excellent spot |
| -50 to -60 | Good | Great spot |
| -60 to -70 | Fair | Workable |
| -70 to -80 | Weak | Marginal |
| < -80 dBm | Poor | Too far |

## Build & Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Build and upload
pio run -t upload

# Serial monitor
pio device monitor
```
