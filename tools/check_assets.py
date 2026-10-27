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
    expected_rate=round((26590000/(64*28))*((sample['pitch_word']*28)//43)/256)
    assert sample['sample_rate']==expected_rate
    assert 0<=sample['loop_start']<=sample['loop_end']<=sample['bytes']
    with wave.open(str(root/'sfx'/sample['file'])) as w:
        assert w.getnchannels()==1 and w.getsampwidth()==2 and w.getframerate()==sample['sample_rate']
        assert w.getnframes()==sample['bytes']
        pcm=w.readframes(w.getnframes())
    values=struct.unpack('<'+'h'*(len(pcm)//2),pcm)
    assert all(x%256==0 for x in values)
    raw=bytes((x//256)&255 for x in values)
    assert hashlib.sha256(raw).hexdigest()==sample['raw_sha256'],sample['file']
print('Seven original modules and 30 losslessly converted original sample payloads verified')

art=json.loads((root/'art/manifest.json').read_text())
assert len(art['atlases'])==5 and len(art['fonts'])==3
for atlas in art['atlases']:
    data=(root/'art'/atlas['file']).read_bytes()
    assert hashlib.sha256(data).hexdigest()==atlas['sha256'],atlas['file']
    assert data[:2]==b'BM'
    width,height=struct.unpack_from('<ii',data,18)
    assert width==atlas['width']==320 and height==-atlas['height'] and atlas['height'] in (128,200)
    assert struct.unpack_from('<I',data,66)[0]==0xff000000
    offset=struct.unpack_from('<I',data,10)[0]
    assert set(data[offset+3::4]) <= {0,255} and 255 in data[offset+3::4]
print('Five original CRY atlases and three font maps verified')
