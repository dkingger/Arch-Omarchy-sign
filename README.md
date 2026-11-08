# Arch-Omarchy-sign

ESP32-C3 Super Mini project that drives a WS2812B LED strip and updates color based on the active desktop theme on your computer.

## Features

- ESP32 firmware with simple HTTP API (`POST /color`)
- WS2812B strip control with configurable pin, LED count and brightness
- Optional segmented LED coloring (different colors per logo/zone)
- Linux host sync script that detects theme changes and pushes matching RGB color
- `systemd --user` service example for always-on sync

## Project structure

- `src/main.cpp`: ESP32 firmware
- `include/secrets.h`: Wi-Fi and LED settings
- `host/theme_sync.py`: desktop theme watcher and color sender
- `host/config.example.json`: color mapping and theme detection config
- `host/systemd/arch-omarchy-sign.service`: optional user service

## 1) Firmware setup (ESP32-C3)

1. Install PlatformIO (VS Code extension or CLI).
2. Edit values in `include/secrets.h`:
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
   - Tune `led_segments` to match your physical LED index ranges
   - Tune `theme_color_map` (`arch` and `omarchy` can be different per theme)
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

Segmented example (two zones):

```bash
curl -X POST http://ESP32_IP/color \
   -H 'Content-Type: application/json' \
   -d '{
      "theme":"manual",
      "brightness":96,
      "segments":[
         {"name":"arch","start":0,"end":54,"r":14,"g":165,"b":233},
         {"name":"omarchy","start":55,"end":109,"r":34,"g":197,"b":94}
      ]
   }'
```

## Notes

- `LED_COUNT` is intentionally configurable in `include/secrets.h` so you can set it later.
- Segment ranges use inclusive indexes (`start` and `end` are both included).
- If Omarchy theme is stored in a custom file, add that path to `theme_file_candidates`.
- For long strips, use proper 5V power injection and consider a level shifter on data line.

## Startup and recovery

On boot Arch lights pure blue and Omarchy pure green (at `LED_BRIGHTNESS`).
Without Wi-Fi, only Arch blinks every 500 ms; Omarchy stays green. The ESP32
keeps trying to connect without rebooting. Startup logo ranges are in
`isArchLed()` in `src/main.cpp` and match the example host configuration.

The host reads stock themes from `/usr/share/omarchy/themes`, user overlays
from `~/.config/omarchy/themes`, and the generated active theme from
`~/.local/state/omarchy/current/theme`. It also supports terminal palettes
in Alacritty, Kitty and Ghostty files. Arch uses the dominant saturated color from the folder icon selected by
`icons.theme`. If no usable PNG folder icon or Pillow is available, Arch uses
the theme primary color (or blue, cyan, magenta, then foreground).
Omarchy uses the theme accent color.
Install Pillow with `sudo pacman -S python-pillow` on Arch Linux.
Per-theme color mappings are used only when no theme color files are found. Themes without usable color files use steady fallback
colors; set `fallback_blink.enabled` to `true` to restore warning blinking.

Colors are resent every 10 seconds, so a rebooted ESP32 receives the current
theme again. Set `resend_interval_seconds` to change this interval.

Run host regression tests with `python3 -m unittest discover -s host -p 'test_*.py'`.

## Turn off when the computer is offline

The ESP32 turns the LEDs off after 15 seconds without a successful `POST /color`.
The host's 10-second resend acts as a heartbeat. The next color update turns the
sign on again. This also turns the sign off if the sync script stops, the computer
sleeps, or the network is unavailable. Startup/Wi-Fi blinking stops after this
timeout too, so the sign stays dark while the computer is off.
Keep `resend_interval_seconds` below 15 seconds.
`GET /health` reports `host_timed_out`; health checks do not keep the LEDs on.

## Desktop color app

The Qt Quick app in [`desktop/`](desktop/README.md) provides separate Arch and
Omarchy color pickers, brightness, manual mode, automatic theme mode and off.
Once installed, open **Arch Sign / Arch-skilt** from the launcher or run
`arch-sign open`. Manual selections persist after the app closes and across
logins until you choose **Follow theme**. The sync service must be running;
after updating `host/theme_sync.py`, restart it with
`systemctl --user restart arch-omarchy-sign.service`.

The app and host exchange control/status JSON atomically under
`~/.local/state/arch-sign/`. Deleting `control.json` restores automatic mode.
Manual and off modes keep sending heartbeats, preserving the sign's behavior
when the computer is shut down. The ESP32 firmware is unchanged by this app.
