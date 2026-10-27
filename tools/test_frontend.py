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
                                           ('legacy6', '1.5 1 0 1 12345 17', 17),
                                           ('current9', '1.5 1 0 1 12345 17 .8 .3 .9', 17)]:
                data = base / name
                prefs = data / 'de-xr/tempest-xr/settings'
                prefs.parent.mkdir(parents=True)
                prefs.write_text(fields + '\n')
                def exercise():
                    key('f')  # The removed hands-free toggle must remain absent.
                    for _ in range(4):
                        key('k')
                    key('o')  # Locked Beastly must remain unavailable.
                    key('Return')
                    key('f')
                    key('p')
                result = run(name, data, exercise)
                saved = prefs.read_text().split()
                assert len(saved) == 9 and saved[3] == '0'
                assert saved[5] == str(unlocked)
                assert float(saved[6]) == (.8 if name == 'current9' else .9)
                assert abs(float(saved[7]) - (.3 if name == 'current9' else .32)) < .001
                assert result['paused'] and result['player_shots'] == 0 and not result['beastly']
                assert result['score'] == (unlocked - 1) * (2600 + 200 * (unlocked - 1))
                records = json.loads(prefs.with_name('records.json').read_text())
                assert records['key'] == 0 and records['keys'][0]['unlocked'] == unlocked
                assert records['keys'][1]['unlocked'] == 1
                assert records['scores'][0]['initials'] == 'YAK'
                print(name + ': old fire preference ignored; native keys, rewards and save passed', flush=True)
            records['keys'][0]['beastly'] = True
            records['scores'][0]['initials'] = 'XR2'
            records['scores'][0]['score'] = 600000
            prefs.with_name('records.json').write_text(json.dumps(records))
            def beastly():
                key('o')
                key('Return')
                key('p')
            result = run('beastly-reload', data, beastly)
            assert result['beastly'] and result['score'] == 2 * 16 * (2600 + 16 * 200)
            restored = json.loads(prefs.with_name('records.json').read_text())
            assert restored['scores'][0]['initials'] == 'XR2' and restored['keys'][0]['beastly']
            print('Beastly unlock and score-table reload passed', flush=True)
        finally:
            connection.close()
            server.terminate()
            server.wait(timeout=5)
