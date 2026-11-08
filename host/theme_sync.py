#!/usr/bin/env python3
import argparse
from collections import Counter
from functools import lru_cache
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
    if omarchy_theme and normalize_theme(omarchy_theme) != "unknown":
        return normalize_theme(omarchy_theme)

    for candidate in ["~/.local/state/omarchy/current/theme.name", *config.get("theme_file_candidates", [])]:
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
    pattern = re.compile(r"([A-Za-z0-9_\-]+)\s*(?:[:=]|\s)\s*[\"']?(#[0-9A-Fa-f]{6})[\"']?")
    colors: Dict[str, Tuple[int, int, int]] = {}
    for match in pattern.finditer(text):
        key = match.group(1).strip().lower().replace("-", "_")
        key = {"color2": "green", "color4": "blue", "color3": "yellow", "color5": "magenta", "color6": "cyan"}.get(key, key)
        colors[key] = parse_hex_color(match.group(2))
    return colors


def _find_theme_directory(theme_slug: str, config: Dict) -> Optional[Path]:
    roots = config.get("theme_roots", ["~/.config/omarchy/themes", "/usr/share/omarchy/themes", "~/.local/share/omarchy/themes"])
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
    # Merge stock colors, user overlays, then the generated active theme.
    roots = config.get("theme_roots", ["~/.config/omarchy/themes", "/usr/share/omarchy/themes", "~/.local/share/omarchy/themes"])
    directories = []
    for root in reversed(roots):
        directory = _find_theme_directory(theme_slug, {"theme_roots": [root]})
        if directory:
            directories.append(directory)
    active = Path("~/.local/state/omarchy/current").expanduser()
    if read_text_if_exists(active / "theme.name") == theme_slug:
        directories.append(active / "theme")
    colors = {}
    for directory in directories:
        for filename in ("alacritty.toml", "kitty.conf", "ghostty.conf", "theme.yml", "theme.yaml", "theme.toml", "colors.yml", "colors.yaml", "colors.toml"):
            text = read_text_if_exists(directory / filename)
            if text:
                colors.update(_extract_named_colors(text))
    return colors


def _derive_palette_from_theme_colors(
    colors: Dict[str, Tuple[int, int, int]],
    default_palette: Dict[str, Tuple[int, int, int]],
) -> Dict[str, Tuple[int, int, int]]:
    if not colors:
        return default_palette

    # Prefer an explicit primary color; Omarchy themes usually provide blue instead.
    arch = (colors.get("primary") or colors.get("blue")
            or colors.get("cyan") or colors.get("magenta")
            or colors.get("foreground")
            or default_palette["arch"])
    omarchy = (colors.get("accent") or colors.get("blue")
               or colors.get("magenta") or colors.get("cyan")
               or default_palette["omarchy"])
    return {"solid": arch, "arch": arch, "omarchy": omarchy}


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


@lru_cache(maxsize=64)
def _folder_image_color(path: str, modified_ns: int) -> Optional[Tuple[int, int, int]]:
    # Cache by modification time so icon updates are picked up without restarting.
    try:
        from PIL import Image
        with Image.open(path) as image:
            pixels = image.convert("RGBA")
            data = pixels.get_flattened_data() if hasattr(pixels, "get_flattened_data") else pixels.getdata()
            colors = Counter((r, g, b) for r, g, b, alpha in data
                             if alpha >= 240 and max(r, g, b) - min(r, g, b) >= 35)
            if colors:
                return colors.most_common(1)[0][0]
    except (ImportError, OSError, ValueError):
        pass
    return None


def folder_color(theme: str, config: Dict) -> Optional[Tuple[int, int, int]]:
    roots = config.get("theme_roots", ["~/.config/omarchy/themes", "/usr/share/omarchy/themes", "~/.local/share/omarchy/themes"])
    directories = []
    active = Path("~/.local/state/omarchy/current").expanduser()
    if read_text_if_exists(active / "theme.name") == theme:
        directories.append(active / "theme")
    for root in roots:
        directory = _find_theme_directory(theme, {"theme_roots": [root]})
        if directory:
            directories.append(directory)
    icon_theme = next((name for directory in directories
                       if (name := read_text_if_exists(directory / "icons.theme"))), "Yaru-blue")
    # Only accept a theme name, never a path supplied by a theme file.
    if not re.fullmatch(r"[A-Za-z0-9_. -]+", icon_theme) or icon_theme in (".", ".."):
        return None
    for root in config.get("icon_roots", ["~/.local/share/icons", "~/.icons", "/usr/share/icons"]):
        for size in ("48x48", "48x48@2x", "256x256", "24x24"):
            path = Path(root).expanduser() / icon_theme / size / "places/folder.png"
            try:
                color = _folder_image_color(str(path), path.stat().st_mtime_ns)
            except OSError:
                continue
            if color is not None:
                return color
    return None


def choose_palette(theme: str, config: Dict) -> Tuple[Dict[str, Tuple[int, int, int]], str]:
    mapping: Dict[str, Any] = {k.lower(): v for k, v in config.get("theme_color_map", {}).items()}
    default_palette = _default_palette_for_theme(theme, config)

    theme_colors = _load_theme_named_colors(theme, config)
    if theme_colors:
        palette = _derive_palette_from_theme_colors(theme_colors, default_palette)
        icon_color = folder_color(theme, config)
        if icon_color is not None:
            palette["arch"] = icon_color
            palette["solid"] = icon_color
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


def control_directory(config: Dict) -> Path:
    return Path(config.get("control_directory", "~/.local/state/arch-sign")).expanduser()


def load_control(config: Dict) -> Dict:
    path = control_directory(config) / "control.json"
    if not path.exists():
        return {"mode": "theme"}
    control = load_config(path)
    if control.get("mode") not in ("theme", "manual", "off"):
        raise ValueError("Unknown sign control mode")
    if control["mode"] == "manual":
        for name in ("arch", "omarchy"):
            parse_hex_color(control[name])
        brightness = control.get("brightness")
        if type(brightness) is not int or not 0 <= brightness <= 255:
            raise ValueError("Manual brightness must be 0-255")
    return control


def resolve_control(config: Dict, control: Dict, theme: str):
    effective_config = dict(config)
    if control["mode"] == "manual":
        arch = parse_hex_color(control["arch"])
        omarchy = parse_hex_color(control["omarchy"])
        effective_config["brightness"] = control["brightness"]
        return effective_config, {"solid": arch, "arch": arch, "omarchy": omarchy}, "manual"
    if control["mode"] == "off":
        return effective_config, dict.fromkeys(("solid", "arch", "omarchy"), (0, 0, 0)), "off"
    preset = control.get("theme_presets", {}).get(theme)
    if preset is not None:
        brightness = preset.get("brightness")
        if type(brightness) is not int or not 0 <= brightness <= 255:
            raise ValueError("Saved theme brightness must be 0-255")
        arch, omarchy = parse_hex_color(preset["arch"]), parse_hex_color(preset["omarchy"])
        effective_config["brightness"] = brightness
        return effective_config, {"solid": arch, "arch": arch, "omarchy": omarchy}, "saved-theme"
    palette, source = choose_palette(theme, config)
    return effective_config, palette, source


def write_status(config: Dict, status: Dict) -> None:
    directory = control_directory(config)
    directory.mkdir(parents=True, exist_ok=True)
    temporary = directory / "status.json.tmp"
    temporary.write_text(json.dumps(status), encoding="utf-8")
    temporary.replace(directory / "status.json")


def main() -> None:
    parser = argparse.ArgumentParser(description="Sync desktop theme to ESP32 LED color")
    parser.add_argument("--config", default="config.json", help="Path to config JSON")
    args = parser.parse_args()

    config_path = Path(args.config).expanduser().resolve()
    config = load_config(config_path)
    poll_interval = float(config.get("poll_interval_seconds", 2))
    blink_interval = float(config.get("fallback_blink", {}).get("interval_seconds", poll_interval))

    last_sent_at = 0.0
    resend_interval = max(1.0, float(config.get("resend_interval_seconds", 10)))
    last_signature: Optional[str] = None
    last_blink_at = 0.0
    blink_phase = False

    print(f"[theme-sync] started with config: {config_path}")
    while True:
        try:
            theme = detect_theme(config) or "unknown"
            control = load_control(config)
            effective_config, palette, source = resolve_control(config, control, theme)

            if source == "fallback" and config.get("fallback_blink", {}).get("enabled", False):
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
                    "brightness": effective_config.get("brightness", 96),
                    "source": source,
                    "palette": {
                        k: list(v) for k, v in sorted(effective_palette.items())
                    },
                },
                sort_keys=True,
            )

            if signature != last_signature or time.monotonic() - last_sent_at >= resend_interval:
                send_color(effective_config, theme if control["mode"] == "theme" else control["mode"], effective_palette)
                print(f"[theme-sync] applied theme={theme} source={source} palette={effective_palette}")
                last_signature = signature
                last_sent_at = time.monotonic()
                write_status(config, {"ok": True, "mode": control["mode"], "theme": theme,
                                      "palette": effective_palette,
                                      "brightness": effective_config.get("brightness", 96),
                                      "updated_at": time.time()})
        except KeyboardInterrupt:
            print("\n[theme-sync] stopped")
            break
        except Exception as exc:
            print(f"[theme-sync] warning: {exc}")
            try:
                write_status(config, {"ok": False, "error": str(exc), "updated_at": time.time()})
            except OSError as status_error:
                print(f"[theme-sync] status warning: {status_error}")

        time.sleep(poll_interval)


if __name__ == "__main__":
    main()
