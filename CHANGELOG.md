# Changelog

## 0.2.0

First packaged desktop release for Arch Linux / Omarchy.

- Ship the Qt Quick desktop app, Python theme sync, systemd user service and
  optional Omarchy bar shortcut in one package.
- Add first-run device setup and a connection settings dialog.
- Migrate legacy checkout-based configuration without losing LED segments or
  saved theme palettes; remove checkout paths from the service.
- Add separate logo colors, desktop pipettes, brightness, manual/theme/off
  modes, and saved/resettable per-theme palettes.
- Match the full Omarchy palette, light/dark mode, language and corner rounding.
- Preview the sign with separately colored backlight halos.
- Add host heartbeats and firmware offline timeout, nonblocking Wi-Fi recovery
  and rollover-safe timers.
- Provide a firmware secrets template and automatic serial-port detection.
- Add clean Arch package CI and regression tests for setup and migration.

Hardware setup and firmware flashing remain separate from desktop installation.
