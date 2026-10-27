#!/usr/bin/env python3
"""Opt-in native SDL input/save test on a private Xvfb display and preference tree.

Run with: uv run --no-project --with python-xlib python tools/test_frontend.py
Does not connect to the user's desktop, runtime or preferences.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from Xlib import display, X, XK
from Xlib.ext import xtest

root = Path(__file__).resolve().parents[1]
out = root / 'artifacts' / 'frontend-test'
out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tempest-frontend-') as temporary:
    base = Path(temporary)
    runtime = base / 'runtime'
    runtime.mkdir(mode=0o700)
    reader, writer = os.pipe()
    with (out / 'xvfb.log').open('w') as log:
        server = subprocess.Popen(['Xvfb', '-displayfd', str(writer), '-screen', '0',
                                   '1024x768x24', '-nolisten', 'tcp', '-noreset'],
                                  pass_fds=(writer,), stdout=log, stderr=log)
        os.close(writer)
        with os.fdopen(reader) as pipe:
            number = pipe.readline().strip()
        assert number, 'Xvfb did not create a display'
        env = dict(os.environ, DISPLAY=':' + number, SDL_VIDEODRIVER='x11',
                   SDL_AUDIODRIVER='dummy', XDG_RUNTIME_DIR=str(runtime),
                   XDG_CONFIG_HOME=str(base / 'config'))
        for name in ('WAYLAND_DISPLAY', 'WAYLAND_SOCKET'):
            env.pop(name, None)
        connection = display.Display(env['DISPLAY'])

        def key(name):
            code = connection.keysym_to_keycode(XK.string_to_keysym(name))
            xtest.fake_input(connection, X.KeyPress, code)
            xtest.fake_input(connection, X.KeyRelease, code)
            connection.sync()
            time.sleep(.08)

        def quit_game():
            code = connection.keysym_to_keycode(XK.string_to_keysym('Control_L'))
            xtest.fake_input(connection, X.KeyPress, code)
            key('q')
            xtest.fake_input(connection, X.KeyRelease, code)
            connection.sync()

        def run(name, data, action):
            report = out / (name + '.json')
            with (out / (name + '.log')).open('w') as client_log:
                process = subprocess.Popen([root / 'build/tempest-xr', '--desktop', '--no-audio',
                                            '--report', report], cwd='/tmp',
                                           env=dict(env, XDG_DATA_HOME=str(data)),
                                           stdout=client_log, stderr=client_log)
                try:
                    deadline = time.monotonic() + 12
                    window = None
                    while time.monotonic() < deadline:
                        for candidate in connection.screen().root.query_tree().children:
                            if 'Tempest' in (candidate.get_wm_name() or ''):
                                window = candidate
                                break
                        if window:
                            break
                        assert process.poll() is None
                        time.sleep(.05)
                    assert window is not None
                    window.set_input_focus(X.RevertToParent, X.CurrentTime)
                    connection.sync()
                    time.sleep(.2)
                    action()
                    quit_game()
                    process.wait(timeout=10)
                    assert process.returncode == 0
                finally:
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=5)
            result = json.loads(report.read_text())
            assert result['gl_error'] == 0 and not result['launch_xr']
            return result

        try:
            for name, fields, unlocked in [('legacy5', '1.5 1 0 1 12345', 1),
                                           ('legacy6', '1.5 1 0 1 12345 35', 35),
                                           ('current9', '1.5 1 0 1 12345 99 .8 .3 .9', 99)]:
                data = base / name
                prefs = data / 'de-xr/tempest-xr/settings'
                prefs.parent.mkdir(parents=True)
                prefs.write_text(fields + '\n')
                def exercise():
                    key('f')
                    key('Right')
                    key('Right')
                    xtest.fake_input(connection, X.ButtonPress, 4)
                    xtest.fake_input(connection, X.ButtonRelease, 4)
                    connection.sync()
                    key('Return')
                    key('f')
                    key('p')
                result = run(name, data, exercise)
                saved = prefs.read_text().split()
                assert len(saved) == 9 and saved[3] == '0'
                assert float(saved[6]) == (.8 if name == 'current9' else .9)
                assert abs(float(saved[7]) - (.3 if name == 'current9' else .32)) < .001
                assert result['paused'] and result['player_shots'] == 0 and not result['beastly']
                assert result['score'] == 0 and result['access_key'] == -1
                records = json.loads(prefs.with_name('records.json').read_text())
                assert records['version'] == 2 and len(records['key_bytes']) == 16
                assert records['key_bytes'][3::4] == [0, 0, 0, 0]
                print(name + ': stage selection absent; normal start is web 1; legacy controls preserved', flush=True)
            legacy = dict(version=1, scores=records['scores'], keys=[
                dict(unlocked=35, beastly=True), dict(unlocked=1),
                dict(unlocked=1), dict(unlocked=1)])
            prefs.with_name('records.json').write_text(json.dumps(legacy))
            def use_key():
                key('k')
                key('Return')
                key('p')
            result = run('key-migration', data, use_key)
            assert result['access_key'] == 0 and result['score'] == 32 * (2600 + 32 * 200)
            restored = json.loads(prefs.with_name('records.json').read_text())
            assert restored['key_bytes'][:4] == [ord('y'), ord('a'), ord('k'), 32]
            assert restored['beastly_unlocked']
            def beastly():
                key('o')
                key('k')
                key('Return')
                key('p')
            result = run('beastly-key-reload', data, beastly)
            assert result['beastly'] and result['score'] == 2 * 32 * (2600 + 32 * 200)
            restored = json.loads(prefs.with_name('records.json').read_text())
            assert restored['key_bytes'][:4] == [ord('y'), ord('a'), ord('k'), 32]
            print('Original checkpoint key migration, key-only resume and Beastly reload passed', flush=True)
        finally:
            connection.close()
            server.terminate()
            server.wait(timeout=5)
