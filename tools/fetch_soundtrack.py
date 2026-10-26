#!/usr/bin/env python3
"""Obtain the seven original Jaguar music modules from the pinned source archive."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

COMMIT = '8b50b467ce7dd7eee475aaf5ae86804d95401ae7'
BASE = f'https://raw.githubusercontent.com/mwenge/tempest2k/{COMMIT}/src/sounds/'
FILES = ['tune13.mod','tune7.mod','tune1.mod','tune3.mod','rave4.mod','tune5.mod','tune12.mod']
HASHES = {
    'rave4.mod':'dd87a5738a84898df34a2aa229b1b695de32e3050ba1749e46a54518ad80ed13',
    'tune1.mod':'5597921aa7d303e8efb941ea94e4dc2bf5a7504bb87314f9aef07c649d4371e7',
    'tune12.mod':'8b887da68bbd8e12b4e44d0d949efcc167b1eebfac0e852ce3ce7753b4f4605e',
    'tune13.mod':'0e10b218a485bec6508b48b3f87b62f7d792850dbe80cbd3efc2f84a80e337a4',
    'tune3.mod':'b93e358e075d13380ec2d0fda0d97aa04f90c1ad5d65629318b30661d9b86758',
    'tune5.mod':'12ed16cd6f6bf1cd869c57aa7f1b3cb01a4602cdae3e124ed36b8fc2b243f6de',
    'tune7.mod':'1c79a6cc4c0f21a6e823b3d54b0e95270b067522a70930c52e036676c510d1e5',
}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, help='Existing tempest2k checkout, instead of downloading')
args = parser.parse_args()
out = Path(__file__).resolve().parents[1]/'assets/music'
out.mkdir(parents=True, exist_ok=True)
manifest = {'source_commit':COMMIT, 'source_url':BASE,
            'credits':'Imagitec Design / Ian Howe, Alastair Lindsay, Kevin Saville, Julian Hodgson / Atari',
            'tracks':[]}
for filename in FILES:
    url = BASE+filename
    data = (args.source/'src/sounds'/filename).read_bytes() if args.source else urllib.request.urlopen(url,timeout=30).read()
    if len(data)<1084 or data[1080:1084]!=b'M.K.':
        raise ValueError(f'{filename}: not the expected ProTracker module')
    if hashlib.sha256(data).hexdigest()!=HASHES[filename]:
        raise ValueError(f'{filename}: checksum differs from the pinned original')
    (out/filename).write_bytes(data)
    title=data[:20].split(b'\0',1)[0].decode('ascii',errors='replace')
    manifest['tracks'].append(dict(file=filename,title=title,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),url=url))
    print(f'{filename}: {title} / {len(data)} bytes')
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
