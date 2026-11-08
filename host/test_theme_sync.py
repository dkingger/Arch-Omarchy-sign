import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch, MagicMock

import theme_sync as sync


class ThemeSyncTests(unittest.TestCase):
    def setUp(self):
        patcher = patch.object(sync, 'folder_color', return_value=None)
        self.folder_mock = patcher.start()
        self.addCleanup(patcher.stop)

    def test_folder_color_replaces_arch_and_keeps_accent(self):
        self.folder_mock.return_value = (188, 101, 189)
        with patch.object(sync, '_load_theme_named_colors', return_value={'primary': (10, 20, 30), 'accent': (1, 2, 3)}):
            palette, _ = sync.choose_palette('custom', {})
        self.assertEqual(palette['arch'], (188, 101, 189))
        self.assertEqual(palette['solid'], (188, 101, 189))
        self.assertEqual(palette['omarchy'], (1, 2, 3))

    def test_folder_image_ignores_transparent_pixels(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'folder.png'
            image = Image.new('RGBA', (4, 4), (0, 0, 0, 0))
            image.putpixel((1, 1), (188, 101, 189, 255))
            image.save(path)
            self.assertEqual(sync._folder_image_color(str(path), path.stat().st_mtime_ns), (188, 101, 189))

    def test_stock_palette_and_user_overlay(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stock, user = root / 'stock', root / 'user'
            for base in (stock, user):
                (base / 'example').mkdir(parents=True)
            (stock / 'example/colors.toml').write_text('blue = "#123456"\ngreen = "#00ff00"')
            (user / 'example/colors.toml').write_text('accent = "#ff0000"')
            palette, source = sync.choose_palette('example', {'theme_roots': [str(user), str(stock)]})
            self.assertEqual(source, 'theme-files')
            self.assertEqual(palette['arch'], (18, 52, 86))
            self.assertEqual(palette['omarchy'], (255, 0, 0))

    def test_terminal_palette(self):
        colors = sync._extract_named_colors('color4 #0000ff\ncolor2 #00ff00\naccent = "#123456"')
        self.assertEqual(colors['blue'], (0, 0, 255))
        self.assertEqual(colors['green'], (0, 255, 0))

    def test_theme_roles_take_priority_over_legacy_mapping(self):
        with patch.object(sync, '_load_theme_named_colors', return_value={'primary': (10, 20, 30), 'blue': (50, 60, 70), 'accent': (1, 2, 3)}):
            palette, _ = sync.choose_palette('custom', {'theme_color_map': {'custom': {'arch': '#abcdef'}}})
        self.assertEqual(palette['arch'], (10, 20, 30))
        self.assertEqual(palette['omarchy'], (1, 2, 3))

    def test_segment_payload_and_brightness(self):
        response = MagicMock()
        response.__enter__.return_value.status = 200
        with patch.object(sync.request, 'urlopen', return_value=response) as send:
            sync.send_color({'esp32_base_url': 'http://device', 'brightness': 999,
                             'led_segments': {'arch_1': [0, 22], 'omarchy_1': [23, 38]}},
                            'example', {'arch': (0, 0, 255), 'omarchy': (0, 255, 0)})
        payload = json.loads(send.call_args.args[0].data)
        self.assertEqual(payload['brightness'], 255)
        self.assertEqual(payload['segments'][0]['b'], 255)
        self.assertEqual(payload['segments'][1]['g'], 255)


if __name__ == '__main__':
    unittest.main()

class ManualControlTests(unittest.TestCase):
    def test_manual_override_and_off_keep_heartbeat_payload(self):
        config = {'brightness': 96}
        effective, palette, source = sync.resolve_control(config, {
            'mode': 'manual', 'arch': '#112233', 'omarchy': '#abcdef', 'brightness': 180}, 'example')
        self.assertEqual(palette['arch'], (17, 34, 51))
        self.assertEqual(palette['omarchy'], (171, 205, 239))
        self.assertEqual(effective['brightness'], 180)
        self.assertEqual(config['brightness'], 96)
        _, off, source = sync.resolve_control(config, {'mode': 'off'}, 'example')
        self.assertEqual(source, 'off')
        self.assertTrue(all(color == (0, 0, 0) for color in off.values()))

    def test_control_missing_defaults_to_theme_and_rejects_bad_input(self):
        with tempfile.TemporaryDirectory() as temp:
            config = {'control_directory': temp}
            self.assertEqual(sync.load_control(config), {'mode': 'theme'})
            Path(temp, 'control.json').write_text(json.dumps({'mode': 'manual', 'arch': 'bad', 'omarchy': '#ffffff', 'brightness': 96}))
            with self.assertRaises(ValueError):
                sync.load_control(config)

    def test_status_is_readable_after_atomic_replace(self):
        with tempfile.TemporaryDirectory() as temp:
            sync.write_status({'control_directory': temp}, {'ok': True})
            self.assertEqual(json.loads(Path(temp, 'status.json').read_text()), {'ok': True})
            self.assertFalse(Path(temp, 'status.json.tmp').exists())


class ThemePresetTests(unittest.TestCase):
    def test_saved_theme_precedes_files_and_reset_restores_them(self):
        control = {'mode': 'theme', 'theme_presets': {
            'retro-82': {'arch': '#112233', 'omarchy': '#abcdef', 'brightness': 145}}}
        with patch.object(sync, 'choose_palette', return_value=({'arch': (1, 2, 3)}, 'theme-files')) as automatic:
            config, palette, source = sync.resolve_control({'brightness': 96}, control, 'retro-82')
            self.assertEqual(palette['arch'], (17, 34, 51))
            self.assertEqual(config['brightness'], 145)
            self.assertEqual(source, 'saved-theme')
            automatic.assert_not_called()
            del control['theme_presets']['retro-82']
            config, palette, source = sync.resolve_control({'brightness': 96}, control, 'retro-82')
            self.assertEqual(palette['arch'], (1, 2, 3))
            self.assertEqual(config['brightness'], 96)
            self.assertEqual(source, 'theme-files')

    def test_manual_and_off_ignore_theme_presets(self):
        presets = {'retro-82': {'arch': '#112233', 'omarchy': '#abcdef', 'brightness': 145}}
        _, palette, source = sync.resolve_control({}, {'mode': 'off', 'theme_presets': presets}, 'retro-82')
        self.assertEqual(palette['arch'], (0, 0, 0))
        self.assertEqual(source, 'off')
