#!/usr/bin/env python3
"""Fetch the original signed 8-bit Jaguar samples and their period metadata."""
import argparse,hashlib,json,struct,urllib.request,wave
from pathlib import Path
COMMIT='8b50b467ce7dd7eee475aaf5ae86804d95401ae7'
BASE=f'https://raw.githubusercontent.com/mwenge/tempest2k/{COMMIT}/src/'
p=argparse.ArgumentParser();p.add_argument('--source',type=Path);a=p.parse_args()
def get(path):return (a.source/'src'/path).read_bytes() if a.source else urllib.request.urlopen(BASE+path,timeout=30).read()
table=get('incbin/sound_samples_table.bin')
out=Path(__file__).resolve().parents[1]/'assets/sfx';out.mkdir(parents=True,exist_ok=True)
manifest={'source_commit':COMMIT,'table_sha256':hashlib.sha256(table).hexdigest(),'samples':[]}
for i in range(30):
    name,priority,period,address,length,repeat,repeat_length=struct.unpack('>20sHHIIII',table[i*40:i*40+40])
    length >>= 8  # Source metadata stores sample lengths as 24:8 fixed point.
    raw=get(f'sounds/samples/{i+1:02d}');assert len(raw)>0
    rate=round(3546895/period)
    pcm=b''.join(struct.pack('<h',(n if n<128 else n-256)*256) for n in raw)
    with wave.open(str(out/f'{i+1:02d}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(pcm)
    label=name.decode('ascii').strip();manifest['samples'].append(dict(file=f'{i+1:02d}.wav',name=label,period=period,sample_rate=rate,bytes=len(raw),table_bytes=length,raw_sha256=hashlib.sha256(raw).hexdigest(),source_url=BASE+f'sounds/samples/{i+1:02d}'))
    print(f'{i+1:02d}: {label} ({rate}Hz)')
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
