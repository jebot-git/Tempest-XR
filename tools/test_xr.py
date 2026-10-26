#!/usr/bin/env python3
"""Opt-in isolated Monado simulated-HMD integration test using native EGL.

Uses a private runtime directory and explicit runtime JSON. Does not change the
system OpenXR runtime or connect to an existing headset session.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
out = root / 'artifacts' / 'xr-test'
out.mkdir(parents=True, exist_ok=True)
(out / 'report.json').unlink(missing_ok=True)
with tempfile.TemporaryDirectory(prefix='tempest-xr-test-') as directory:
    runtime = Path(directory)
    runtime.chmod(0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=directory+'/config',
               XDG_DATA_HOME=directory+'/data', SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
               XR_RUNTIME_JSON='/usr/share/openxr/1/openxr_monado.json',
               SIMULATED_ENABLE='1', XRT_COMPOSITOR_NULL='1', XRT_COMPOSITOR_NULL_FPS='30',
               IPC_EXIT_WHEN_IDLE='false')
    binary = runtime / 'monado-service'
    shutil.copyfile(shutil.which('monado-service'), binary)
    binary.chmod(0o700)
    with (out / 'monado.log').open('w') as log:
        server = subprocess.Popen([binary], env=env, stdout=log, stderr=log, stdin=subprocess.PIPE)
        try:
            deadline = time.monotonic()+20
            while not (runtime / 'monado_comp_ipc').exists():
                if server.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError('Monado did not start; see artifacts/xr-test/monado.log')
                time.sleep(.1)
            result = subprocess.run([root/'build/tempest-xr','--xr','--effects-demo','--level','1',
                                     '--frames','400','--no-audio','--capture',out/'mirror.ppm'],
                                    env=env, capture_output=True, text=True, timeout=45)
            text = result.stdout + result.stderr
            (out / 'client.log').write_text(text)
            matches = re.findall(r'OpenXR frames=(\d+) stereo_frames=(\d+)',text)
            assert result.returncode==0, text
            assert matches and int(matches[-1][1])>=50, text
            assert 'XR unavailable:' not in text and 'XR session ended:' not in text, text
            motion=re.search(r'warp_frames=(\d+) max_warp_distance=([\d.]+) jump_frames=(\d+)',text)
            assert motion and int(motion[1])>=100 and float(motion[2])>13.9 and int(motion[3])>=30,text
            report = dict(passed=True, fixture='Monado simulated HMD / null compositor / native EGL',
                          frames=int(matches[-1][0]),stereo_frames=int(matches[-1][1]),
                          warp_frames=int(motion[1]),max_warp_distance=float(motion[2]),jump_frames=int(motion[3]),
                          physical_headset_tested=False)
            extra={}
            for name,options in [('enemies',['--enemy-demo']),('rings',['--bonus-demo','0']),('tunnel',['--bonus-demo','1']),('psychedelic',['--bonus-demo','2'])]:
                run=subprocess.run([root/'build/tempest-xr','--xr',*options,'--frames','90','--no-audio','--capture',out/(name+'.ppm')],env=env,capture_output=True,text=True,timeout=20)
                log=run.stdout+run.stderr;(out/(name+'.log')).write_text(log)
                stats=re.findall(r'OpenXR frames=(\d+) stereo_frames=(\d+)',log)
                assert run.returncode==0 and stats and int(stats[-1][1])>=80,log
                assert 'XR unavailable:' not in log and 'XR session ended:' not in log,log
                extra[name]={'frames':int(stats[-1][0]),'stereo_frames':int(stats[-1][1])}
            report['extra_scenarios']=extra
            (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(json.dumps(report))
        finally:
            server.terminate()
            try:
                server.wait(timeout=8)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
