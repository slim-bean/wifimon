# WiFiMon

ESP32 M5StickC Plus 1.1 based realtime WiFi signal strength monitor with CSI waterfall display.

Designed to help find optimal placement for WiFi devices by walking around and observing signal strength and channel quality in real time.

## Features

- **Real-time RSSI monitoring** — scans continuously as fast as the ESP32 allows
- **Color-coded signal bar** and large dBm readout with quality label
- **Rolling signal history graph** — see trends as you walk
- **CSI waterfall spectrogram** — visualizes per-subcarrier channel state in real time
- **CSI equalizer view** — 64-channel graphic equalizer showing live subcarrier amplitudes
- **Connected and passive CSI modes** — connects to APs with known credentials for high update rates, falls back to passive sniffing otherwise
- **Channel congestion indicator** — shows how many networks overlap your target's channel
- **Geiger counter mode** — buzzer beeps faster and higher-pitched as signal strengthens (toggle with Button B)
- **Network selection** — scan and pick your target SSID on boot
- **Moving average smoothing** for stable, readable values
- **Min/Max tracking** across your session

## Hardware

- M5StickC Plus 1.1

## Controls

| Button | Network List | Monitor / CSI Views |
|--------|-------------|---------------------|
| A (front) | Select network | Back to network list |
| B (side) | Next network | Toggle buzzer |
| PWR (short press) | — | Cycle view: RSSI → Waterfall → Equalizer |

## Signal Strength Reference

| RSSI | Quality | Repeater Suitability |
|------|---------|---------------------|
| > -50 dBm | Great | Excellent spot |
| -50 to -60 | Good | Great spot |
| -60 to -70 | Fair | Workable |
| -70 to -80 | Weak | Marginal |
| < -80 dBm | Poor | Too far |

## CSI Views

The CSI (Channel State Information) views show the WiFi channel's frequency response in real time. Two visualizations are available: a **waterfall spectrogram** and a **graphic equalizer**. Cycle between views with the PWR button.

### Connected vs passive mode

CSI data comes from analyzing the physical layer of received WiFi packets. The ESP32 supports two modes for collecting this data:

**Connected mode `[CONN]`** — When credentials are available for the selected network (see Setup below), the device connects to the AP and sends ~50 UDP packets/sec to the gateway. Each response generates CSI data, giving consistent 20-50+ pkt/s update rates. The CSI data comes exclusively from the target AP since the device is associated with it.

**Passive mode `[PASV]`** — When no credentials are available, the device uses promiscuous mode to sniff packets on the channel. Packets are filtered by the AP's BSSID (matching on the last 5 bytes of the MAC address to account for routers using multiple virtual interfaces). Update rates are lower and less consistent (~3-5 pkt/s), limited mostly to beacon frames.

The current mode and packet rate are shown on the CSI view header and footer.

### How OFDM subcarriers work

WiFi (802.11n) uses OFDM (Orthogonal Frequency-Division Multiplexing) to transmit data. Instead of using one wide radio signal, it splits the 20MHz channel into 64 narrow frequency slices called **subcarriers**. Each subcarrier carries a small piece of data independently.

Not all 64 subcarriers carry data:
- **25 lower subcarriers** (negative frequencies, left of center)
- **26 upper subcarriers** (positive frequencies, right of center)
- **DC subcarrier** (center frequency) — always null, cannot carry data because it would create a constant voltage offset
- **Guard band subcarriers** (edges) — left empty to avoid interference with adjacent channels

The display shows the 51 active subcarriers split into left and right halves with a thin vertical divider marking the DC null point in the center. This divider represents the channel's center frequency — the frequency your router is actually tuned to.

### What it shows

Each horizontal line represents one received packet's frequency response across the 51 active subcarriers. Color maps amplitude: black (weak) through blue, cyan, green, yellow, to red (strong). New data appears at the top and scrolls down. The waterfall pauses when no packets are received, so it only advances with real information.

It is normal for subcarriers near the center (close to DC) to appear slightly dimmer than those at the edges. This is a characteristic of the OFDM signal, not a problem with your WiFi.

### Reading the display

- **Uniform bright colors across all subcarriers** — clean channel with minimal frequency-selective fading, good placement
- **Patchy/jagged rows with deep color nulls** — heavy multipath causing some frequencies to cancel out
- **Stable vertical stripes** — consistent channel response over time, stable environment
- **Rapidly shifting patterns** — movement or interference in the environment

### Practical use

For finding the best placement for a WiFi device, the RSSI monitor view is more directly actionable (stronger signal = better spot). The CSI view provides a deeper look at channel quality — it shows whether the signal is arriving cleanly or being distorted by reflections and obstacles. A spot with good RSSI but jagged/unstable CSI may perform worse than a spot with slightly lower RSSI but clean, stable CSI.

### Improving update rate in passive mode

In passive mode, the waterfall updates each time a matching packet is received. To get faster updates, generate traffic on the target network from another device:

- **Ping from a phone** — install a terminal app (Termux on Android, iSH on iPhone) and run `ping -i 0.2 <router_ip>`
- **Stream video** — play a video on any device connected to the network
- **Run a speed test** — generates a burst of sustained traffic

For the best experience, add your network's credentials (see below) to enable connected mode.

## Setup

### WiFi credentials (optional)

To enable connected CSI mode with high update rates, add your WiFi credentials:

1. Copy `src/credentials.h.example` to `src/credentials.h`
2. Add your networks:

```cpp
static const WiFiCredential WIFI_CREDENTIALS[] = {
    {"MyNetwork", "mypassword"},
    {"OtherNetwork", "otherpassword"},
};
```

`credentials.h` is gitignored and will not be committed. Networks without matching credentials will use passive mode automatically.

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
