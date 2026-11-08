#!/usr/bin/env python3
"""Configure an installed Arch Sign package without modifying Omarchy themes."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from urllib.parse import urlsplit


def validate_url(value):
    value = value.strip().rstrip('/')
    parsed = urlsplit(value)
    if (parsed.scheme not in ('http', 'https') or not parsed.hostname or
            parsed.username or parsed.password or parsed.path or parsed.query or parsed.fragment):
        raise ValueError('Use an ESP32 address like http://192.168.1.120')
    if any(char.isspace() for char in value):
        raise ValueError('The address must not contain spaces')
    try:
        port = parsed.port
    except ValueError as exc:
        raise ValueError('Invalid port') from exc
    if port is not None and not 1 <= port <= 65535:
        raise ValueError('Invalid port')
    return value


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as file:
            temporary = Path(file.name)
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def configure(url, *, home=None, share=None, migrate=None, bar=False, start=True, runner=subprocess.run):
    home = Path.home() if home is None else Path(home)
    share = Path('/usr/share/arch-sign') if share is None else Path(share)
    url = validate_url(url)
    path = home / '.config/arch-sign/config.json'
    source = path if path.exists() else Path(migrate) if migrate else share / 'config.example.json'
    config = json.loads(source.read_text())
    if not isinstance(config, dict):
        raise ValueError('Configuration must be a JSON object')
    config['esp32_base_url'] = url
    if path.exists():
        backup = path.with_name('config.json.before-setup')
        if not backup.exists():
            shutil.copy2(path, backup)
    atomic_write(path, (json.dumps(config, indent=2) + '\n').encode())
    # Replace legacy checkout-bound units with the packaged service definition.
    unit = home / '.config/systemd/user/arch-omarchy-sign.service'
    packaged_unit = (share / 'arch-omarchy-sign.service').read_bytes()
    if unit.exists() and unit.read_bytes() != packaged_unit:
        backup = unit.with_name(unit.name + '.before-package')
        if not backup.exists():
            shutil.copy2(unit, backup)
    atomic_write(unit, packaged_unit)
    if start:
        for arguments in (['systemctl', '--user', 'daemon-reload'],
                          ['systemctl', '--user', 'enable', 'arch-omarchy-sign.service'],
                          ['systemctl', '--user', 'restart', 'arch-omarchy-sign.service']):
            runner(arguments, check=True, capture_output=True, text=True, timeout=15)
    if bar:
        if not shutil.which('omarchy') or not shutil.which('omarchy-shell'):
            raise RuntimeError('Omarchy shell is required for the optional bar shortcut. The service is configured.')
        plugin = home / '.config/omarchy/plugins/local.arch-sign'
        shutil.copytree(share / 'bar', plugin, dirs_exist_ok=True)
        for arguments in (['omarchy-shell', 'shell', 'rescanPlugins'],
                          ['omarchy', 'plugin', 'enable', 'local.arch-sign', '--section', 'centerEnd', '--index', '0']):
            runner(arguments, check=True, capture_output=True, text=True, timeout=15)
    return {'ok': True, 'config': str(path), 'url': url, 'started': start, 'bar': bar}


def main():
    parser = argparse.ArgumentParser(description='Configure the Arch Sign device and background service')
    parser.add_argument('--url', required=True, help='ESP32 base URL, e.g. http://192.168.1.120')
    parser.add_argument('--migrate-config', type=Path, help='Import a legacy config, including LED segments')
    parser.add_argument('--bar', action='store_true', help='Add the optional Omarchy clock shortcut')
    parser.add_argument('--no-start', action='store_true', help='Write settings without starting the service')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        result = configure(args.url, migrate=args.migrate_config, bar=args.bar, start=not args.no_start)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        detail = getattr(exc, 'stderr', None) or str(exc)
        print(json.dumps({'ok': False, 'error': detail}))
        raise SystemExit(1)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
