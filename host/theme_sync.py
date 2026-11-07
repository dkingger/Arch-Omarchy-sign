#!/usr/bin/env python3
import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, Optional, Tuple
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

    omarchy_theme = run_command(["omarchy", "theme", "current"])
    if omarchy_theme:
        return normalize_theme(omarchy_theme)

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


def _extract_named_colors(text: str) -> Dict[str, Tuple[int, int, int]]:
    # Supports simple TOML/YAML-like lines such as: accent = "#ff00aa"
    pattern = re.compile(r"([A-Za-z0-9_\-]+)\s*[:=]\s*[\"']?(#[0-9A-Fa-f]{6})[\"']?")
    colors: Dict[str, Tuple[int, int, int]] = {}
    for match in pattern.finditer(text):
        key = match.group(1).strip().lower().replace("-", "_")
        colors[key] = parse_hex_color(match.group(2))
    return colors


def _find_theme_directory(theme_slug: str, config: Dict) -> Optional[Path]:
    roots = config.get("theme_roots", ["~/.config/omarchy/themes"])
    candidates = {
        theme_slug,
        theme_slug.replace("-", "_"),
        theme_slug.replace("_", "-"),
    }

    for root_raw in roots:
        root = Path(root_raw).expanduser()
        if not root.exists() or not root.is_dir():
            continue

        for candidate in candidates:
            path = root / candidate
            if path.exists() and path.is_dir():
                return path

    return None


def _load_theme_named_colors(theme_slug: str, config: Dict) -> Dict[str, Tuple[int, int, int]]:
    theme_dir = _find_theme_directory(theme_slug, config)
    if theme_dir is None:
        return {}

    color_files = [
        theme_dir / "colors.toml",
        theme_dir / "colors.yaml",
        theme_dir / "colors.yml",
        theme_dir / "theme.toml",
        theme_dir / "theme.yaml",
        theme_dir / "theme.yml",
    ]

    colors: Dict[str, Tuple[int, int, int]] = {}
    for file_path in color_files:
        if not file_path.exists() or not file_path.is_file():
            continue

        text = file_path.read_text(encoding="utf-8", errors="ignore")
        colors.update(_extract_named_colors(text))

    return colors


def _derive_palette_from_theme_colors(
    colors: Dict[str, Tuple[int, int, int]],
    default_palette: Dict[str, Tuple[int, int, int]],
) -> Dict[str, Tuple[int, int, int]]:
    if not colors:
        return default_palette

    arch = (
        colors.get("arch")
        or colors.get("accent")
        or colors.get("primary")
        or colors.get("blue")
        or colors.get("magenta")
        or colors.get("cyan")
        or default_palette.get("arch")
        or default_palette.get("solid")
    )

    omarchy = (
        colors.get("omarchy")
        or colors.get("green")
        or colors.get("yellow")
        or colors.get("orange")
        or colors.get("red")
        or colors.get("foreground")
        or default_palette.get("omarchy")
        or default_palette.get("solid")
    )

    if arch == omarchy:
        omarchy = (
            colors.get("yellow")
            or colors.get("green")
            or colors.get("magenta")
            or colors.get("cyan")
            or default_palette.get("omarchy")
            or omarchy
        )

    solid = colors.get("solid") or colors.get("accent") or default_palette.get("solid")
    return {
        "solid": solid,
        "arch": arch,
        "omarchy": omarchy,
    }


def _default_palette_for_theme(theme: str, config: Dict) -> Dict[str, Tuple[int, int, int]]:
    default_color = config.get("default_color", "#22c55e")

    if "dark" in theme:
        base_hex = config.get("fallback_dark_color", default_color)
    elif "light" in theme:
        base_hex = config.get("fallback_light_color", default_color)
    else:
        base_hex = default_color

    base_rgb = parse_hex_color(base_hex)

    fallback_palette = config.get("fallback_palette", {})
    palette: Dict[str, Tuple[int, int, int]] = {
        "solid": base_rgb,
        "arch": parse_hex_color(fallback_palette.get("arch", base_hex)),
        "omarchy": parse_hex_color(fallback_palette.get("omarchy", base_hex)),
    }
    return palette


def choose_palette(theme: str, config: Dict) -> Tuple[Dict[str, Tuple[int, int, int]], str]:
    mapping: Dict[str, Any] = {k.lower(): v for k, v in config.get("theme_color_map", {}).items()}
    default_palette = _default_palette_for_theme(theme, config)

    theme_colors = _load_theme_named_colors(theme, config)
    if theme_colors:
        palette = _derive_palette_from_theme_colors(theme_colors, default_palette)
        explicit_theme_entry = mapping.get(theme)
        if isinstance(explicit_theme_entry, dict):
            for name, hex_color in explicit_theme_entry.items():
                if isinstance(hex_color, str):
                    palette[name.lower()] = parse_hex_color(hex_color)
        return palette, "theme-files"

    palette = dict(default_palette)

    theme_entry = mapping.get(theme)
    if theme_entry is None:
        return palette, "fallback"

    if isinstance(theme_entry, str):
        color = parse_hex_color(theme_entry)
        palette["solid"] = color
        palette["arch"] = color
        palette["omarchy"] = color
        return palette, "mapped"

    if isinstance(theme_entry, dict):
        for name, hex_color in theme_entry.items():
            if isinstance(hex_color, str):
                palette[name.lower()] = parse_hex_color(hex_color)

        if "solid" in palette:
            palette.setdefault("arch", palette["solid"])
            palette.setdefault("omarchy", palette["solid"])

    return palette, "mapped"


def _blink_palette(config: Dict, phase: bool) -> Dict[str, Tuple[int, int, int]]:
    blink_config = config.get("fallback_blink", {})
    on_hex = blink_config.get("on", {}) if isinstance(blink_config, dict) else {}
    off_hex = blink_config.get("off", {}) if isinstance(blink_config, dict) else {}

    on_arch = parse_hex_color(on_hex.get("arch", "#ff2d2d"))
    on_omarchy = parse_hex_color(on_hex.get("omarchy", "#ffd400"))
    off_arch = parse_hex_color(off_hex.get("arch", "#150000"))
    off_omarchy = parse_hex_color(off_hex.get("omarchy", "#151000"))

    if phase:
        return {"arch": on_arch, "omarchy": on_omarchy}
    return {"arch": off_arch, "omarchy": off_omarchy}


def apply_fallback_blink(
    palette: Dict[str, Tuple[int, int, int]],
    config: Dict,
    phase: bool,
) -> Dict[str, Tuple[int, int, int]]:
    blinking = _blink_palette(config, phase)
    effective = dict(palette)
    effective["arch"] = blinking["arch"]
    effective["omarchy"] = blinking["omarchy"]
    effective["solid"] = blinking["arch"]
    return effective


def send_color(config: Dict, theme: str, palette: Dict[str, Tuple[int, int, int]]) -> None:
    base_url = config["esp32_base_url"].rstrip("/")
    brightness = int(config.get("brightness", 96))
    brightness = max(0, min(brightness, 255))

    segment_map = config.get("led_segments", {})
    payload: Dict[str, Any] = {
        "theme": theme,
        "brightness": brightness,
    }

    segments = []
    if isinstance(segment_map, dict):
        for segment_name, bounds in segment_map.items():
            if not (isinstance(bounds, list) and len(bounds) == 2):
                continue

            segment_key = segment_name.lower()
            segment_group = segment_key.split("_", 1)[0]
            color = (
                palette.get(segment_key)
                or palette.get(segment_group)
                or palette.get("solid")
            )
            if color is None:
                continue

            segments.append(
                {
                    "name": segment_name,
                    "start": int(bounds[0]),
                    "end": int(bounds[1]),
                    "r": color[0],
                    "g": color[1],
                    "b": color[2],
                }
            )

    if segments:
        payload["segments"] = segments
    else:
        fallback = palette.get("solid")
        if fallback is None and palette:
            fallback = next(iter(palette.values()))
        if fallback is None:
            fallback = (34, 197, 94)

        payload["r"] = fallback[0]
        payload["g"] = fallback[1]
        payload["b"] = fallback[2]

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
    blink_interval = float(config.get("fallback_blink", {}).get("interval_seconds", poll_interval))

    last_theme: Optional[str] = None
    last_signature: Optional[str] = None
    last_blink_at = 0.0
    blink_phase = False

    print(f"[theme-sync] started with config: {config_path}")
    while True:
        try:
            theme = detect_theme(config) or "unknown"
            palette, source = choose_palette(theme, config)

            if source == "fallback":
                now = time.monotonic()
                if now - last_blink_at >= max(0.1, blink_interval):
                    blink_phase = not blink_phase
                    last_blink_at = now
                effective_palette = apply_fallback_blink(palette, config, blink_phase)
            else:
                effective_palette = palette

            signature = json.dumps(
                {
                    "theme": theme,
                    "source": source,
                    "palette": {
                        k: list(v) for k, v in sorted(effective_palette.items())
                    },
                },
                sort_keys=True,
            )

            if signature != last_signature:
                send_color(config, theme, effective_palette)
                print(f"[theme-sync] applied theme={theme} source={source} palette={effective_palette}")
                last_signature = signature
                last_theme = theme
        except KeyboardInterrupt:
            print("\n[theme-sync] stopped")
            break
        except Exception as exc:
            print(f"[theme-sync] warning: {exc}")

        time.sleep(poll_interval)


if __name__ == "__main__":
    main()
