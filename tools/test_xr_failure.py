#!/usr/bin/env python3
"""Inject a runtime failure without contacting a physical headset or user session."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--game', type=Path, required=True)
p.add_argument('--runtime', type=Path, required=True)
a = p.parse_args()
with tempfile.TemporaryDirectory(prefix='tempest-xr-failure-') as directory:
    report = Path(directory) / 'report.json'
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_DATA_HOME=directory,
               SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
               LD_PRELOAD=str(a.runtime.resolve()), TEMPEST_TEST_FAIL_FRAME='8')
    env.pop('WAYLAND_DISPLAY', None)
    result = subprocess.run([a.game.resolve(), '--xr', '--demo', '--no-audio',
                             '--frames', '30', '--report', report], env=env,
                            capture_output=True, text=True, timeout=15)
    log = result.stdout + result.stderr
    assert result.returncode == 0 and 'XR session ended:' in log, log
    stats = json.loads(report.read_text())
    assert stats['xr_entries'] == 1 and stats['paused'], stats
    assert not stats['captured'] and not stats['xr_intent'], stats
    assert not stats['relative_mouse'] and stats['gl_error'] == 0, stats
    print('Runtime failure pauses play and releases capture and XR intent before exit.')
