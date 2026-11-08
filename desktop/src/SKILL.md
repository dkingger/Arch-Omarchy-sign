---
name: arch-sign
description: Control the user's Arch/Omarchy LED sign through its local background service.
---
Use `arch-sign status --json` to inspect the last send result and timestamp.
Set both logos with `arch-sign manual '#0000ff' '#00ff00' 96 --json`.
Brightness is 0–255. Arguments are Arch color, Omarchy color, brightness.
Use `arch-sign theme --json` to restore automatic theme sync, or `arch-sign off --json` to switch off.
Commands save choices atomically; the service applies them within its polling interval.
A successful save does not guarantee delivery: inspect status afterward.
Do not edit ESP32 Wi-Fi credentials or stop the background service to change colors.

Use `arch-sign save-theme '<active-theme>' --json` to save the current draft
colors and brightness for the active theme. This applies manual colors too.
Use `arch-sign reset-theme '<active-theme>' --json` to remove only that theme's
preset and switch to automatic theme colors. The name must match current status.
