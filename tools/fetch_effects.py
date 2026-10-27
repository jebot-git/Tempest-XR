#!/usr/bin/env python3
"""Fetch source PCM, using the included Imagitec driver's actual pitch conversion."""
import argparse
import hashlib
import json
import re
import struct
import urllib.request
import wave
from pathlib import Path

COMMIT = '8b50b467ce7dd7eee475aaf5ae86804d95401ae7'
BASE = f'https://raw.githubusercontent.com/mwenge/tempest2k/{COMMIT}/src/'
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path)
a = p.parse_args()


def get(path):
    if a.source:
        return (a.source / 'src' / path).read_bytes()
    return urllib.request.urlopen(BASE + path, timeout=30).read()


# yak.s includes this relocated driver at $4040. Check executable instructions,
# rather than relying on the misleading "Period" column / channel-count comment.
driver_data = get('dat/moomoo.dat')
driver = b''.join(
    struct.pack('>I', int(n, 16))
    for line in driver_data.decode('ascii').splitlines()
    if re.match(r'\s*DC.L\s', line, re.I)
    for n in re.findall(r'\$([0-9a-fA-F]+)', line.split(';')[0])
)
assert hashlib.sha256(driver).hexdigest() == '37c466acc81e94c66273dd5c3584ece04d46e2912f53ba8da7db946e3d6d87bc'
# move.l #27,SCLK; move.l #21,SMODE
assert driver[0x4146-0x4040:0x415a-0x4040] == bytes.fromhex('23fc0000001b00f1a15023fc0000001500f1a154')
# PLAYFX2: mulu #28,d3; divu #43,d3; and.l #$ffff,d3
assert driver[0x4440-0x4040:0x444e-0x4040] == bytes.fromhex('c6fc001c86fc002b02830000ffff')
# DBRA loops over d6+1 slots: two with MOD playback, six without it.
assert driver[0x4394-0x4040:0x4396-0x4040] == bytes.fromhex('7c01')
assert driver[0x43a2-0x4040:0x43a4-0x4040] == bytes.fromhex('7c05')
# DSP starts at $5ed4: add r3,r19 (pitch to phase); shrq #8,r19 (sample index).
assert struct.unpack_from('>H', driver, 0x5ed4-0x4040+0x8a)[0] == 0x0073
assert struct.unpack_from('>H', driver, 0x5ed4-0x4040+0xac)[0] == 0x6513

table = get('incbin/sound_samples_table.bin')
out = Path(__file__).resolve().parents[1] / 'assets/sfx'
out.mkdir(parents=True, exist_ok=True)
manifest = {
    'source_commit': COMMIT,
    'table_sha256': hashlib.sha256(table).hexdigest(),
    'playback': {
        'clock_hz': 26590000,
        'sclk': 27,
        'pitch_conversion': 'floor(pitch*28/43), 8.8 phase increment',
        'driver': BASE + 'dat/moomoo.dat',
        'driver_sha256': hashlib.sha256(driver_data).hexdigest(),
        'decoded_driver_sha256': hashlib.sha256(driver).hexdigest(),
    },
    'samples': [],
}
specs = []
for i in range(30):
    name, priority, pitch, address, length, repeat, repeat_length = struct.unpack(
        '>20sHHIIII', table[i*40:i*40+40])
    raw = get(f'sounds/samples/{i+1:02d}')
    assert raw
    # Length and repeat length are 24:8 fixed point; repeat is an absolute address.
    loop_start = max(0, repeat-address) if repeat_length else 0
    loop_end = min(len(raw), loop_start+(repeat_length >> 8)) if repeat_length else 0
    assert 0 <= loop_start <= loop_end <= len(raw)
    specs.append((pitch, priority, loop_start, loop_end))
    # SCLK=27 and 32 stereo bit clocks per frame: clock/(64*(SCLK+1)).
    rate = round((26590000/(64*28))*((pitch*28)//43)/256)
    pcm = b''.join(struct.pack('<h', (n if n < 128 else n-256)*256) for n in raw)
    with wave.open(str(out / f'{i+1:02d}.wav'), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)
    label = name.decode('ascii').strip()
    manifest['samples'].append(dict(
        file=f'{i+1:02d}.wav', name=label, priority=priority, pitch_word=pitch,
        sample_rate=rate, loop_start=loop_start, loop_end=loop_end, bytes=len(raw),
        table_bytes=length >> 8, raw_sha256=hashlib.sha256(raw).hexdigest(),
        source_url=BASE + f'sounds/samples/{i+1:02d}'))
    print(f'{i+1:02d}: {label} ({rate}Hz)')
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
header = [
    '// Generated from the original sound_samples_table.bin; Jeff Minter / Atari.',
    f'// {BASE}incbin/sound_samples_table.bin', '#pragma once', '#include <array>',
    'namespace tempest {',
    '// Included Imagitec driver: SCLK=27, positive pitch word, 8.8 phase.',
    'constexpr int effectStep(int pitch) { return pitch*28/43; }',
    'struct SampleSpec {int pitch,priority,loopStart,loopEnd;};',
    'inline constexpr std::array<SampleSpec,30> sourceSamples={{',
]
header += ['{%d,%d,%d,%d},' % spec for spec in specs]
header += ['}};', '}']
(out.parents[1] / 'src/sample_data.hpp').write_text('\n'.join(header)+'\n')
