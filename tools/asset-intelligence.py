"""Read-only original BMD identity graph and runtime evidence join (schema 1).

Reuses the topology harness's RARC reader. No copyrighted bytes are exported.
Replacement filenames are candidates, never proof of active renderer selection.
"""
import argparse
import hashlib
import importlib.util
import json
import re
import struct
from pathlib import Path

spec = importlib.util.spec_from_file_location('bmd', Path(__file__).with_name('inspect-bmd.py'))
bmd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bmd)
MASK = (1 << 64) - 1
P1, P2, P3, P4, P5 = 11400714785074694791, 14029467366897019727, 1609587929392839161, 9650029242287828579, 2870177450012600261

def xxh64(data):
    """XXH64 seed zero, matching Aurora's original base-mip source key."""
    def rol(x, n):
        x &= MASK
        return ((x << n) | (x >> (64-n))) & MASK
    def step(a, x):
        return rol(a + x * P2, 31) * P1 & MASK
    n, at = len(data), 0
    if n >= 32:
        v = [(P1+P2)&MASK, P2, 0, (-P1)&MASK]
        while at+32 <= n:
            for i, x in enumerate(struct.unpack_from('<4Q', data, at)):
                v[i] = step(v[i], x)
            at += 32
        h = sum(rol(v[i], r) for i, r in enumerate((1,7,12,18))) & MASK
        for x in v:
            h = ((h ^ step(0, x))*P1+P4)&MASK
    else:
        h = P5
    h = (h+n)&MASK
    while at+8 <= n:
        h = (rol(h ^ step(0, int.from_bytes(data[at:at+8], 'little')),27)*P1+P4)&MASK
        at += 8
    if at+4 <= n:
        h = (rol(h ^ (int.from_bytes(data[at:at+4], 'little')*P1 & MASK),23)*P2+P3)&MASK
        at += 4
    while at < n:
        h = rol(h ^ (data[at]*P5 & MASK),11)*P1 & MASK
        at += 1
    h ^= h >> 33
    h = h*P2 & MASK
    h ^= h >> 29
    h = h*P3 & MASK
    return f'{h ^ (h >> 32):016x}'

def fnv(data):
    h = 14695981039346656037
    for x in data:
        h = ((h ^ x)*1099511628211)&MASK
    return f'{h:016x}'

def take(data, at, size):
    if at < 0 or size < 0 or at+size > len(data):
        raise ValueError('metadata exceeds resource bounds')
    return data[at:at+size]

def u16(data, at):
    return int.from_bytes(take(data, at, 2), 'big')

def u32(data, at):
    return int.from_bytes(take(data, at, 4), 'big')

def names(data, at, count):
    if not at or u16(data, at) != count:
        raise ValueError('missing or inconsistent name table')
    result = []
    for i in range(count):
        begin = at + u16(data, at+4+i*4+2)
        end = data.find(b'\0', begin, min(begin+256, len(data)))
        if end < begin:
            raise ValueError('unterminated name')
        result.append(take(data, begin, end-begin).decode('ascii', 'backslashreplace'))
    return result

def model(data, archive, name):
    data = bytes(data)
    if len(data) > 8*1024*1024 or take(data, 0, 8) != b'J3D2bmd3':
        raise ValueError('unsupported model (only bounded J3D2bmd3)')
    if u32(data, 8) != len(data) or u32(data, 12) > 64:
        raise ValueError('invalid model length or block count')
    blocks, at = {}, 32
    for _ in range(u32(data, 12)):
        tag, size = take(data, at, 4), u32(data, at+4)
        if size < 8 or tag in blocks:
            raise ValueError('invalid or duplicate block')
        blocks[tag] = take(data, at, size)
        at += size
    if b'MAT3' not in blocks or b'TEX1' not in blocks:
        raise ValueError('MAT3/TEX1 unavailable')
    mat, tex = blocks[b'MAT3'], blocks[b'TEX1']
    mc, tc = u16(mat, 8), u16(tex, 8)
    if mc > 256 or tc > 256:
        raise ValueError('metadata count limit')
    mn, tn = names(mat, u32(mat, 20), mc), names(tex, u32(tex, 16), tc)
    textures = []
    # GX block geometry, including padded edge blocks. Base mip only.
    formats = {0:(8,8,32), 1:(8,4,32), 2:(8,4,32), 3:(4,4,32),
               4:(4,4,32), 5:(4,4,32), 6:(4,4,64), 8:(8,8,32),
               9:(8,4,32), 10:(4,4,32), 14:(8,8,32)}
    for i in range(tc):
        h = u32(tex, 12)+i*32
        header = take(tex, h, 32)
        fmt, w, height = header[0], u16(header,2), u16(header,4)
        if fmt not in formats or not w or not height:
            raise ValueError('unsupported texture format/dimensions')
        bw,bh,bs = formats[fmt]
        pixels = take(tex,h+u32(header,28),((w+bw-1)//bw)*((height+bh-1)//bh)*bs)
        key = {'width':w, 'height':height, 'gx_format':fmt, 'texture_hash':xxh64(pixels)}
        if fmt in (8,9,10):
            indices = ([n for x in pixels for n in (x&15,x>>4)] if fmt==8 else
                       list(pixels) if fmt==9 else [u16(pixels,j)&0x3fff for j in range(0,len(pixels),2)])
            lo,hi = min(indices),max(indices)
            if hi >= u16(header,10):
                raise ValueError('palette reference exceeds declared colors')
            key['tlut_hash'] = xxh64(take(tex,h+u32(header,12)+lo*2,(hi-lo+1)*2))
        textures.append({'index':i,'name':tn[i],'source_key':key,'provenance':'original-TEX1-base-mip',
                         'replacement_selection':'unavailable-through-TextureService-v1'})
    materials = []
    for i in range(mc):
        init = u32(mat,12)+u16(mat,u32(mat,16)+i*2)*0x14c
        take(mat,init,0x14c)
        slots=[]
        for s in range(8):
            ref=u16(mat,init+0x84+s*2)
            value=65535 if ref==65535 else u16(mat,u32(mat,0x48)+ref*2)
            if value != 65535 and value >= tc:
                raise ValueError('material texture index out of range')
            slots.append(value)
        materials.append({'index':i,'name':mn[i],'texture_slots':slots,'provenance':'original-MAT3-remap'})
    return {'id':f'{archive}/{name}#fnv1a64:{fnv(data)}','archive':archive,'resource':name,
            'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),
            'materials':materials,'textures':textures,'confidence':'established-original-file',
            'runtime_creations':0,'rendered_object':'not-observed'}

def join_runtime(graph, lines):
    by_id={m['id']:m for m in graph['models']}
    for line in lines:
        if 'AssetGraph ' not in line:
            continue
        event=json.loads(line.split('AssetGraph ',1)[1])
        if not isinstance(event, dict) or event.get('schema') != 1:
            raise ValueError('unsupported runtime schema')
        if event.get('event') == 'unsupported':
            graph.setdefault('runtime_limitations', []).append(event.get('reason', 'unspecified'))
        item=by_id.get(event.get('id'))
        if not item:
            continue
        if event['event']=='creation':
            item['runtime_creations'] += 1
        if event['event']=='resource':
            item['resource_load_observations']=item.get('resource_load_observations',0)+1
        if event['event']=='material':
            if not isinstance(event.get('material'), int) or not 0 <= event['material'] < len(item['materials']):
                raise ValueError('runtime material index out of range')
            expected=item['materials'][event['material']]['texture_slots']
            slots=event['texture_slots']
            if not isinstance(slots,list) or len(slots)!=8 or any(v is not None and v!=expected[i] for i,v in enumerate(slots)):
                raise ValueError('runtime material slots differ from original')
            item.setdefault('runtime_materials',[]).append(event['material'])
        if event['event']=='archive-delete':
            item['archive_delete_observations']=item.get('archive_delete_observations',0)+1
    for item in by_id.values():
        item['runtime_materials']=sorted(set(item.get('runtime_materials',[])))
    graph['runtime_limitations']=sorted(set(graph.get('runtime_limitations',[])))

def replacements(graph, directory):
    if not directory.is_dir():
        raise ValueError('replacement directory unavailable')
    # Exact keys only; wildcard priority and mip sidecars are explicitly excluded.
    pattern=re.compile(r'tex1_(\d+)x(\d+)_([0-9a-f]{16})(?:_([0-9a-f]{16}))?_(\d+)\.(dds|png)$',re.I)
    candidates=[]
    for path in sorted(directory.rglob('*')):
        if not path.is_file(): continue
        match=pattern.fullmatch(path.name)
        if not match: continue
        w,h,tex,tlut,fmt,_=match.groups()
        key={'width':int(w),'height':int(h),'gx_format':int(fmt),'texture_hash':tex.lower()}
        if tlut: key['tlut_hash']=tlut.lower()
        candidates.append((key,path.relative_to(directory).as_posix()))
    for m in graph['models']:
        for t in m['textures']:
            t['replacement_candidates']=[p for key,p in candidates if key==t['source_key']]

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('archive',type=Path)
    parser.add_argument('--runtime-log',type=Path)
    parser.add_argument('--replacement-dir',type=Path)
    parser.add_argument('--midnafx-revision',required=True)
    parser.add_argument('--backend',default='offline')
    parser.add_argument('--platform',default='offline')
    parser.add_argument('--scene',default='not-observed')
    args=parser.parse_args()
    graph={'schema':1,'midnafx_revision':args.midnafx_revision,
           'dusklight_revision':'edf42c6a7202647b56dd2fcdef02d17671bc814b',
           'backend':args.backend,'platform':args.platform,'scene':args.scene,'models':[], 'failures':[]}
    try:
        for _,name,data in bmd.archive_entries(args.archive.read_bytes()):
            if not name.endswith('.bmd'): continue
            try: graph['models'].append(model(data,args.archive.name,name))
            except ValueError as error: graph['failures'].append({'resource':name,'reason':str(error)})
        if not graph['models'] and not graph['failures']:
            graph['failures'].append({'reason':'no supported BMD resources'})
        graph['models'].sort(key=lambda m:m['id'])
        if args.runtime_log: join_runtime(graph,args.runtime_log.read_text(errors='replace').splitlines())
        if args.replacement_dir: replacements(graph,args.replacement_dir)
    except (ValueError, OSError, struct.error, KeyError) as error:
        graph['failures'].append({'reason':str(error)})
    print(json.dumps(graph,sort_keys=True,indent=2))
    return bool(graph['failures'])

if __name__=='__main__':
    raise SystemExit(main())
