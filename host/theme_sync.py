#!/usr/bin/env python3
import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path
from typing import Dict, Optional, Tuple
from urllib import request


def load_config(path: Path) -> Dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def read_text_if_exists(path: Path) -> Optional[str]:
    if not path.exists() or not path.is_file():
        return None
    content = path.read_text(encoding="utf-8", errors="ignore").strip()
    return content or None


def run_command(args: list[str]) -> Optional[str]:
    try:
        out = subprocess.check_output(args, text=True, timeout=1.5)
    except Exception:
        return None
    return out.strip() or None


def normalize_theme(raw: str) -> str:
    lowered = raw.lower().strip().strip("\"'")
    lowered = re.sub(r"\s+", "-", lowered)
    return lowered


def detect_theme(config: Dict) -> Optional[str]:
    env_theme = os.getenv("OMARCHY_THEME") or os.getenv("ARCH_OMARCHY_THEME")
    if env_theme:
        return normalize_theme(env_theme)

    for candidate in config.get("theme_file_candidates", []):
        raw = read_text_if_exists(Path(candidate).expanduser())
        if raw:
            return normalize_theme(raw)

    gnome_color_scheme = run_command([
        "gsettings",
        "get",
        "org.gnome.desktop.interface",
        "color-scheme",
    ])
    if gnome_color_scheme:
        return normalize_theme(gnome_color_scheme)

    gtk_theme = run_command([
        "gsettings",
        "get",
        "org.gnome.desktop.interface",
        "gtk-theme",
    ])
    if gtk_theme:
        return normalize_theme(gtk_theme)

    return None


def parse_hex_color(value: str) -> Tuple[int, int, int]:
    cleaned = value.strip().lstrip("#")
    if len(cleaned) != 6:
        raise ValueError(f"Expected 6-char hex color, got: {value}")
    return int(cleaned[0:2], 16), int(cleaned[2:4], 16), int(cleaned[4:6], 16)


def choose_color(theme: str, config: Dict) -> Tuple[int, int, int]:
    mapping = {k.lower(): v for k, v in config.get("theme_color_map", {}).items()}
    default_color = config.get("default_color", "#22c55e")

    if theme in mapping:
        return parse_hex_color(mapping[theme])

    if "dark" in theme:
        return parse_hex_color(config.get("fallback_dark_color", default_color))
    if "light" in theme:
        return parse_hex_color(config.get("fallback_light_color", default_color))

    return parse_hex_color(default_color)


def send_color(config: Dict, theme: str, rgb: Tuple[int, int, int]) -> None:
    base_url = config["esp32_base_url"].rstrip("/")
    brightness = int(config.get("brightness", 96))
    brightness = max(0, min(brightness, 255))

    payload = {
        "theme": theme,
        "r": rgb[0],
        "g": rgb[1],
        "b": rgb[2],
        "brightness": brightness,
    }

    req = request.Request(
        url=f"{base_url}/color",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    with request.urlopen(req, timeout=2.5) as resp:
        if resp.status < 200 or resp.status >= 300:
            raise RuntimeError(f"ESP32 replied with status {resp.status}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Sync desktop theme to ESP32 LED color")
    parser.add_argument("--config", default="config.json", help="Path to config JSON")
    args = parser.parse_args()

    config_path = Path(args.config).expanduser().resolve()
    config = load_config(config_path)
    poll_interval = float(config.get("poll_interval_seconds", 2))

    last_theme: Optional[str] = None

    print(f"[theme-sync] started with config: {config_path}")
    while True:
        try:
            theme = detect_theme(config) or "unknown"
            if theme != last_theme:
                rgb = choose_color(theme, config)
                send_color(config, theme, rgb)
                print(f"[theme-sync] applied theme={theme} color={rgb}")
                last_theme = theme
        except KeyboardInterrupt:
            print("\n[theme-sync] stopped")
            break
        except Exception as exc:
            print(f"[theme-sync] warning: {exc}")

        time.sleep(poll_interval)


if __name__ == "__main__":
    main()
