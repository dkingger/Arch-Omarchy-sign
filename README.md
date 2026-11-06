# Arch-Omarchy-sign

ESP32-C3 Super Mini project that drives a WS2812B LED strip and updates color based on the active desktop theme on your computer.

## Features

- ESP32 firmware with simple HTTP API (`POST /color`)
- WS2812B strip control with configurable pin, LED count and brightness
- Linux host sync script that detects theme changes and pushes matching RGB color
- `systemd --user` service example for always-on sync

## Project structure

- `src/main.cpp`: ESP32 firmware
- `include/secrets.example.h`: Wi-Fi and LED settings template
- `host/theme_sync.py`: desktop theme watcher and color sender
- `host/config.example.json`: color mapping and theme detection config
- `host/systemd/arch-omarchy-sign.service`: optional user service

## 1) Firmware setup (ESP32-C3)

1. Install PlatformIO (VS Code extension or CLI).
2. Copy `include/secrets.example.h` to `include/secrets.h`.
3. Edit values in `include/secrets.h`:
   - `WIFI_SSID`, `WIFI_PASSWORD`
   - `LED_PIN`
   - `LED_COUNT` (set this to your strip length)
   - `LED_BRIGHTNESS`
4. Wire strip:
   - ESP32 `GND` -> LED strip `GND`
   - ESP32 data pin (`LED_PIN`) -> LED strip `DIN`
   - External 5V supply -> LED strip `5V` (recommended for many LEDs)
   - Share ground between ESP32 and strip power supply

### Build and upload

```bash
cd /home/johsdahl/Projects/Arch-Omarchy-sign
pio run
pio run -t upload
pio device monitor
```

After boot, note the ESP32 IP shown in serial output.

## 2) Host sync setup (Linux desktop)

1. Copy config template:

```bash
cd /home/johsdahl/Projects/Arch-Omarchy-sign/host
cp config.example.json config.json
```

2. Edit `host/config.json`:
   - Set `esp32_base_url` to your ESP32 IP, for example `http://192.168.1.120`
   - Tune `theme_color_map`
   - If needed, adjust `theme_file_candidates` to your Omarchy theme path

3. Run sync script manually:

```bash
python3 /home/johsdahl/Projects/Arch-Omarchy-sign/host/theme_sync.py --config /home/johsdahl/Projects/Arch-Omarchy-sign/host/config.json
```

When theme changes, script sends a new RGB color to the ESP32.

## 3) Optional: run as systemd user service

```bash
mkdir -p ~/.config/systemd/user
cp /home/johsdahl/Projects/Arch-Omarchy-sign/host/systemd/arch-omarchy-sign.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now arch-omarchy-sign.service
systemctl --user status arch-omarchy-sign.service
```

## API quick test

```bash
curl -X POST http://ESP32_IP/color \
  -H 'Content-Type: application/json' \
  -d '{"theme":"manual","r":255,"g":80,"b":20,"brightness":96}'
```

## Notes

- `LED_COUNT` is intentionally configurable in `include/secrets.h` so you can set it later.
- If Omarchy theme is stored in a custom file, add that path to `theme_file_candidates`.
- For long strips, use proper 5V power injection and consider a level shifter on data line.
