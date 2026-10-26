#!/usr/bin/env python3
"""Verify original music bytes and lossless conversion of effect samples."""
from pathlib import Path
import hashlib,json,struct,wave
root=Path(__file__).resolve().parents[1]/'assets'
music=json.loads((root/'music/manifest.json').read_text())
assert len(music['tracks'])==7
for track in music['tracks']:
    data=(root/'music'/track['file']).read_bytes()
    assert len(data)==track['bytes'] and hashlib.sha256(data).hexdigest()==track['sha256'],track['file']
effects=json.loads((root/'sfx/manifest.json').read_text())
assert len(effects['samples'])==30
for sample in effects['samples']:
    with wave.open(str(root/'sfx'/sample['file'])) as w:
        assert w.getnchannels()==1 and w.getsampwidth()==2 and w.getframerate()==sample['sample_rate']
        assert w.getnframes()==sample['bytes']
        pcm=w.readframes(w.getnframes())
    values=struct.unpack('<'+'h'*(len(pcm)//2),pcm)
    assert all(x%256==0 for x in values)
    raw=bytes((x//256)&255 for x in values)
    assert hashlib.sha256(raw).hexdigest()==sample['raw_sha256'],sample['file']
print('Seven original modules and 30 losslessly converted original sample payloads verified')
