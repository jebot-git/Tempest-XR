#!/usr/bin/env python3
"""Import original shaded polygon faces, intensities and animated claw meshes."""
import argparse,hashlib,re,urllib.request
from pathlib import Path
COMMIT='8b50b467ce7dd7eee475aaf5ae86804d95401ae7'
URL=f'https://raw.githubusercontent.com/mwenge/tempest2k/{COMMIT}/src/obj2d.s'
p=argparse.ArgumentParser();p.add_argument('--source',type=Path);a=p.parse_args()
data=a.source.read_bytes() if a.source else urllib.request.urlopen(URL,timeout=30).read()
src='\n'.join(line.split(';')[0] for line in data.decode('latin1').splitlines())
names=['s_flipper','s_fliptank','s_fliptank2','fbpiece1','fbpiece2']+[f'spuls{i}' for i in range(1,7)]+['pshot','pwrlaser','mirr','blueflipper','s_flip1','s_flip2','hornm1','hornm2','hornm3','adroid']+[f'{prefix}{i}' for prefix in ['sclaw','gsclaw'] for i in range(8)]
out=['// Generated original polygon data: Jeff Minter / Atari.',f'// {URL}',f'// SHA256 {hashlib.sha256(data).hexdigest()}',
     '#pragma once','#include <vector>','#include <string_view>','namespace tempest {',
     'struct MeshVertex {float x,y,light;};','struct MeshFace {int color;std::vector<MeshVertex> vertices;};',
     'struct SourceMesh {std::string_view name;std::vector<MeshFace> faces;};','inline const std::vector<SourceMesh> sourceMeshes={']
for name in names:
    start=re.search(rf'^{name}:',src,re.M);assert start,name
    records=[]
    for m in re.finditer(r'\bdc\.([lw])\s+([^\n]+)',src[start.end():],re.I):
        values=[int(x.strip().replace('$','0x'),16 if '$' in x else 10) for x in m[2].split(',')]
        records.append((m[1],values))
    faces=[];face_count=records[0][1][0]
    words=[n for kind,values in records[1:] if kind=='w' for n in values];cursor=0
    for _ in range(face_count):
        color=words[cursor];cursor+=1;vertices=[]
        while len(vertices)<3 or words[cursor]!=0:
            index,light=words[cursor:cursor+2];vertices.append((index,light));cursor+=2
        cursor+=1;faces.append((color,vertices))
    required=(max(index for _,v in faces for index,_ in v)+1)*2
    xy=words[cursor:cursor+required]
    assert len(xy)%2==0 and len(xy)>4
    encoded=[]
    for color,vertices in faces:
        points=[]
        for index,light in vertices:
            assert index*2+1<len(xy),(name,index,xy)
            if light>255:light>>=8
            points.append('{%.6ff,%.6ff,%.6ff}'%((xy[index*2]-9)/6,-(xy[index*2+1]-9)/6,light/255))
        encoded.append('{%d,{%s}}'%(color,','.join(points)))
    out.append('{"%s",{%s}},'%(name,','.join(encoded)))
out+=['};','inline const SourceMesh* sourceMesh(std::string_view name){for(const auto& mesh:sourceMeshes)if(mesh.name==name)return &mesh;return nullptr;}','}']
(Path(__file__).resolve().parents[1]/'src/meshes.hpp').write_text('\n'.join(out)+'\n')
print(f'Imported {len(names)} original shaded/animated meshes')
