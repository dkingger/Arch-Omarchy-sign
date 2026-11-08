import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

spec = importlib.util.spec_from_file_location('sign_setup', Path(__file__).with_name('setup.py'))
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class SetupTests(unittest.TestCase):
    def test_url_validation(self):
        self.assertEqual(setup.validate_url('http://192.168.1.120/'), 'http://192.168.1.120')
        self.assertEqual(setup.validate_url('http://[::1]:8080'), 'http://[::1]:8080')
        for value in ('file:///tmp/test', 'http://host/path', 'http://u:p@host', 'http://host:99999', 'http://a b', 'http://host?x=1'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                setup.validate_url(value)

    def test_installs_and_migrates_without_losing_user_data(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            home, share = root / 'home', root / 'share'
            share.mkdir()
            (share / 'config.example.json').write_text(json.dumps({'led_segments': {'arch': [0, 22]}}))
            unit = Path(__file__).parent / 'systemd/arch-omarchy-sign.service'
            (share / unit.name).write_bytes(unit.read_bytes())
            legacy = root / 'legacy.json'
            legacy.write_text(json.dumps({'esp32_base_url': 'http://old', 'led_segments': {'arch': [9, 19]}, 'brightness': 42}))
            presets = home / '.local/state/arch-sign/control.json'
            presets.parent.mkdir(parents=True)
            presets.write_text('{"theme_presets":{"nord":{"arch":"#112233"}}}')
            previous_presets = presets.read_bytes()
            runner = Mock()
            result = setup.configure('http://device', home=home, share=share, migrate=legacy, runner=runner)
            self.assertTrue(result['ok'])
            config_path = home / '.config/arch-sign/config.json'
            config = json.loads(config_path.read_text())
            self.assertEqual(config['led_segments'], {'arch': [9, 19]})
            self.assertEqual(config['brightness'], 42)
            self.assertEqual(presets.read_bytes(), previous_presets)
            self.assertEqual(runner.call_count, 3)
            setup.configure('http://second', home=home, share=share, runner=runner)
            self.assertEqual(json.loads(config_path.with_name('config.json.before-setup').read_text())['esp32_base_url'], 'http://device')
            self.assertNotIn('Projects', (home / '.config/systemd/user' / unit.name).read_text())

    def test_bad_url_writes_nothing(self):
        with tempfile.TemporaryDirectory() as temporary:
            home = Path(temporary) / 'home'
            with self.assertRaises(ValueError):
                setup.configure('file:///etc/passwd', home=home)
            self.assertFalse(home.exists())
