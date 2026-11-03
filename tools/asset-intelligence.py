"""Read-only J3D identity graphs (schema 1) and resource-tree census (schema 2).

Reuses the topology harness's RARC reader. No copyrighted bytes are exported.
Replacement filenames are candidates, never proof of active renderer selection.
"""
import argparse
import collections
import hashlib
import importlib.util
import json
import re
import struct
from pathlib import Path
from urllib.parse import quote

spec = importlib.util.spec_from_file_location('bmd', Path(__file__).with_name('inspect-bmd.py'))
bmd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bmd)
MASK = (1 << 64) - 1
MODEL_FORMATS = (b'J3D2bmd2', b'J3D2bmd3', b'J3D2bdl3', b'J3D2bdl4', b'J3D2bmt2', b'J3D2bmt3')
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
    if not at:
        return [None] * count
    if u16(data, at) != count:
        raise ValueError('missing or inconsistent name table')
    result = []
    for i in range(count):
        begin = at + u16(data, at+4+i*4+2)
        end = data.find(b'\0', begin, min(begin+256, len(data)))
        if end < begin:
            raise ValueError('unterminated name')
        result.append(take(data, begin, end-begin).decode('ascii', 'backslashreplace'))
    return result

def texture_key(data, at, standalone=False, warnings=None):
    """Original padded base mip and used palette range, never replacement pixels."""
    header = take(data, at, 32)
    fmt, w, height = header[0], u16(header, 2), u16(header, 4)
    formats = {0:(8,8,32), 1:(8,4,32), 2:(8,4,32), 3:(4,4,32),
               4:(4,4,32), 5:(4,4,32), 6:(4,4,64), 8:(8,8,32),
               9:(8,4,32), 10:(4,4,32), 14:(8,8,32)}
    if fmt not in formats or not w or not height:
        raise ValueError('unsupported texture format/dimensions')
    image_offset = struct.unpack_from('>i', header, 28)[0]
    if not image_offset and not standalone:
        return None  # TEX1 may reference a texture supplied by another table.
    bw, bh, bs = formats[fmt]
    pixels = take(data, at + (image_offset or 32), ((w+bw-1)//bw)*((height+bh-1)//bh)*bs)
    key = {'width':w, 'height':height, 'gx_format':fmt, 'texture_hash':xxh64(pixels)}
    if fmt in (8,9,10):
        indices = ([n for x in pixels for n in (x&15,x>>4)] if fmt==8 else
                   list(pixels) if fmt==9 else [u16(pixels,j)&0x3fff for j in range(0,len(pixels),2)])
        lo, hi = min(indices), max(indices)
        palette_offset = struct.unpack_from('>i', header, 12)[0]
        if not palette_offset:
            return None
        palette = take(data, at+palette_offset, u16(header, 10)*2)
        if hi >= u16(header, 10):
            # Aurora deliberately keeps the zero-initialized TLUT source hash
            # when padded indices exceed the declared palette. Never read past it.
            key['tlut_hash'] = '0000000000000000'
            if warnings is not None:
                warnings.append('palette range exceeds declared colors; native zero TLUT hash')
        else:
            key['tlut_hash'] = xxh64(palette[lo*2:(hi+1)*2])
    return key


def model(data, archive, name):
    data = bytes(data)
    magic = take(data, 0, 8)
    if len(data) > 8*1024*1024 or magic not in MODEL_FORMATS:
        raise ValueError('unsupported J3D format or resource size')
    if u32(data, 8) != len(data) or u32(data, 12) > 64:
        raise ValueError('invalid model length or block count')
    blocks, at = {}, 32
    for _ in range(u32(data, 12)):
        tag, size = take(data, at, 4), u32(data, at+4)
        if size < 8 or tag in blocks:
            raise ValueError('invalid or duplicate block')
        blocks[tag] = take(data, at, size)
        at += size
    mat_tag = b'MAT2' if magic in (b'J3D2bmd2', b'J3D2bmt2') else b'MAT3'
    if mat_tag not in blocks or (b'TEX1' not in blocks and magic not in (b'J3D2bmt2', b'J3D2bmt3')):
        raise ValueError('material/texture table unavailable')
    mat, tex = blocks[mat_tag], blocks.get(b'TEX1')
    init_size, slot_offset, texture_table = (0x138, 0x70, 0x3c) if mat_tag == b'MAT2' else (0x14c, 0x84, 0x48)
    mc, tc = u16(mat, 8), u16(tex, 8) if tex else 0
    if mc > 256 or tc > 256:
        raise ValueError('metadata count limit')
    mn, tn = names(mat, u32(mat, 20), mc), names(tex, u32(tex, 16), tc) if tex else []
    textures = []
    for i in range(tc):
        h = u32(tex, 12)+i*32
        warnings = []
        key = texture_key(tex, h, warnings=warnings)
        textures.append({'index':i,'name':tn[i],'source_key':key,'provenance':'original-TEX1-base-mip',
                         'source_status':'available' if key else 'external-texture-unresolved',
                         'replacement_selection':'unavailable-through-TextureService-v1'})
        if warnings:
            textures[-1]['warnings'] = warnings
    materials = []
    for i in range(mc):
        init = u32(mat,12)+u16(mat,u32(mat,16)+i*2)*init_size
        take(mat,init,init_size)
        slots=[]
        for s in range(8):
            ref=u16(mat,init+slot_offset+s*2)
            value=65535 if ref==65535 else u16(mat,u32(mat,texture_table)+ref*2)
            if tex is not None and value != 65535 and value >= tc:
                raise ValueError('material texture index out of range')
            slots.append(value)
        materials.append({'index':i,'name':mn[i],'texture_slots':slots,'provenance':f'original-{mat_tag.decode()}-remap',
                          'texture_reference_status':'local-table' if tex is not None else 'external-table-unresolved'})
    return {'id':f'{archive}/{name}#fnv1a64:{fnv(data)}','archive':archive,'resource':name,
            'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),
            'materials':materials,'textures':textures,'confidence':'established-original-file',
            'format':magic.decode('ascii'), 'blocks':[tag.decode('ascii', 'backslashreplace') for tag in blocks],
            'runtime_creations':0,'rendered_object':'not-observed'}

def join_runtime(graph, lines):
    by_id={m['id']:m for m in graph['models']}
    if len(by_id) != len(graph['models']):
        raise ValueError('ambiguous duplicate runtime resource identity')
    labels, encoded, providers = {}, [], {}
    for line in lines:
        if 'AssetProvider ' in line:
            provider=json.loads(line.split('AssetProvider ',1)[1])
            if not isinstance(provider,dict) or provider.get('schema')!=1 or provider.get('event')!='handle-created':
                raise ValueError('unsupported provider schema')
            registration=provider.get('registration')
            if type(registration) is not int or registration<=0:
                raise ValueError('invalid provider registration')
            if not isinstance(provider.get('file'), str) or not replacement_key(provider['file']):
                raise ValueError('unsupported provider file')
            fingerprint=provider.get('path_fnv1a64')
            if not isinstance(fingerprint,str) or not re.fullmatch(r'[0-9a-f]{16}',fingerprint):
                raise ValueError('invalid provider path fingerprint')
            if registration in providers and providers[registration]!=provider:
                raise ValueError('conflicting provider registration')
            providers[registration]=provider
            continue
        if 'AssetDraw ' in line:
            event=json.loads(line.split('AssetDraw ',1)[1])
            if not isinstance(event,dict) or event.get('schema')!=1 or event.get('event')!='encoded':
                raise ValueError('unsupported draw schema')
            if not isinstance(event.get('label'),str) or not re.fullmatch(r'MidnaFXAssetV1:\d+:\d+',event['label']):
                raise ValueError('invalid encoded draw label')
            if not all(type(event.get(k)) is int and event[k]>=0 for k in ('frame','vertices','indices','instances')):
                raise ValueError('invalid encoded draw counts')
            if not event['instances'] or not (event['vertices'] or event['indices']):
                raise ValueError('empty encoded draw')
            if 'slot0' in event:
                slot=event['slot0']
                if not isinstance(slot,dict) or not all(type(slot.get(k)) is int for k in ('bound','replacement','width','height','mips')):
                    raise ValueError('invalid slot-0 binding')
                if slot['bound'] not in (0,1) or slot['replacement'] not in (0,1):
                    raise ValueError('invalid slot-0 flags')
                if (slot['bound'] and not all(slot[k]>0 for k in ('width','height','mips'))) or (not slot['bound'] and any(slot.values())):
                    raise ValueError('inconsistent slot-0 binding')
                if 'registration' in slot:
                    registration=slot['registration']
                    if type(registration) is not int or registration<0 or (registration and not slot['replacement']):
                        raise ValueError('inconsistent binding registration')
            encoded.append(event)
            continue
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
        if event['event']=='draw-label':
            label=event.get('label','')
            index=event.get('material')
            if not isinstance(label,str) or not re.fullmatch(r'MidnaFXAssetV1:\d+:\d+',label) or label in labels:
                raise ValueError('invalid or reused draw label')
            if type(index) is not int or not 0<=index<len(item['materials']):
                raise ValueError('draw material index out of range')
            labels[label]=(item,index)
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
    matched=set()
    for event in encoded:
        binding=labels.get(event.get('label'))
        if not binding: continue
        item,index=binding
        matched.add(event['label'])
        item['runtime_encoded_draws']=item.get('runtime_encoded_draws',0)+1
        item.setdefault('runtime_encoded_materials',[]).append(index)
        item['rendered_object']='native-WebGPU-draw-encoded; visibility-and-GPU-completion-unobserved'
        if 'slot0' in event:
            observation={'material':index,**event['slot0']}
            provider=providers.get(observation.get('registration'))
            if provider:
                texture_index=item['materials'][index]['texture_slots'][0]
                texture=next((t for t in item['textures'] if t['index']==texture_index),None)
                key=texture['source_key'] if texture else None
                if not key or replacement_key(provider['file']) != key:
                    raise ValueError('provider differs from original slot-0 source')
                observation['provider']={k:provider[k] for k in ('file','path_fnv1a64')}
            observations=item.setdefault('runtime_slot0_bindings',[])
            if observation not in observations: observations.append(observation)
    for label,(item,index) in labels.items():
        item['runtime_draw_labels']=item.get('runtime_draw_labels',0)+1
        if label not in matched: item['runtime_unencoded_labels']=item.get('runtime_unencoded_labels',0)+1
    for item in by_id.values():
        if 'runtime_slot0_bindings' in item:
            item['runtime_slot0_bindings'].sort(key=lambda s:(s['material'],s['bound'],s['replacement'],s['width'],s['height'],s['mips']))
        if 'runtime_encoded_materials' in item:
            item['runtime_encoded_materials']=sorted(set(item['runtime_encoded_materials']))

def replacement_key(filename):
    match = re.fullmatch(r'tex1_(\d+)x(\d+)_([0-9a-f]{16})(?:_([0-9a-f]{16}))?_(\d+)\.(dds|png)', filename, re.I)
    if not match:
        return None
    w, h, tex, tlut, fmt, _ = match.groups()
    if not int(w) or not int(h):
        return None
    key = {'width':int(w), 'height':int(h), 'gx_format':int(fmt), 'texture_hash':tex.lower()}
    if tlut:
        key['tlut_hash'] = tlut.lower()
    return key


def all_textures(graph):
    for item in graph['models']:
        yield from item['textures']
    for item in graph.get('resources', []):
        yield from item.get('textures', [])


def replacements(graph, directory):
    if not directory.is_dir():
        raise ValueError('replacement directory unavailable')
    # Exact keys only; wildcard priority and mip sidecars are explicitly excluded.
    candidates=collections.defaultdict(list)
    for path in sorted(directory.rglob('*')):
        if not path.is_file(): continue
        key=replacement_key(path.name)
        if key:
            candidates[tuple(sorted(key.items()))].append(path.relative_to(directory).as_posix())
    for texture in all_textures(graph):
        key=texture['source_key']
        texture['replacement_candidates']=candidates.get(tuple(sorted(key.items())), []) if key else []
    if 'coverage' in graph:
        graph['coverage']['replacement_candidate_textures'] = sum(
            bool(t['replacement_candidates']) for t in all_textures(graph))
        graph['coverage']['replacement_selection'] = 'unavailable-offline'


def census(root):
    """Inventory every local resource; parse supported metadata without live scanning."""
    if not root.is_dir():
        raise ValueError('resource directory unavailable')
    graph = {'schema':2, 'scope':'offline-resource-tree', 'models':[], 'resources':[],
             'archives':[], 'failures':[], 'limitations':[
                 'Identity does not establish actor ownership, visibility, draw attribution or material semantics.',
                 'Animation and other unparsed formats have byte identity only.',
                 'Replacement filenames are candidates, not resolved provider selection.',
                 'Nested archives expand to depth 8; deeper containers are reported as incomplete.',
                 'Runtime joins require a single archive; basename-only runtime IDs cannot disambiguate a resource tree.']}

    def archive_record(location, raw, depth, source, expanded=None):
        record = {'path':location, 'bytes':len(raw), 'sha256':hashlib.sha256(raw).hexdigest(),
                  'status':'complete', 'members':0, 'source':source, 'depth':depth}
        graph['archives'].append(record)
        if depth > 8:
            record['status'] = 'incomplete'
            raise ValueError('nested archive depth exceeds 8')
        try:
            entries = list(bmd.archive_records(raw if expanded is None else expanded))
        except (ValueError, struct.error):
            record['status'] = 'invalid'
            raise
        for tag, member, index, data in sorted(entries, key=lambda e:e[1]):
            row = resource(location, member, index, data, depth)
            row['node_tag'] = tag
            record['members'] += 1

    def resource(archive, member, index, raw, depth=0):
        # Escape delimiters, including percent itself, before composing identities.
        # Already composed archive locations are not encoded again for nested data.
        location = f'{archive}!{quote(member, safe="/")}' if archive else quote(member, safe='/')
        row = {'id':location, 'archive':archive, 'member':member, 'entry':index, 'format':Path(member).suffix.lower() or 'untyped',
               'bytes':len(raw), 'sha256':hashlib.sha256(raw).hexdigest(), 'status':'identity-only'}
        graph['resources'].append(row)
        try:
            data = bmd.yaz0(raw)
            row['encoding'] = 'Yaz0' if raw[:4] == b'Yaz0' else 'raw'
            row['expanded_bytes'] = len(data)
            magic = data[:8]
            suffix = Path(member).suffix.lower()
            if magic[:4] in (b'J3D1', b'J3D2'):
                row['format'] = magic.decode('ascii', 'backslashreplace')
            else:
                row['format'] = suffix or 'untyped'
            if magic in MODEL_FORMATS:
                item = model(data, archive or '.', member)
                item['id'] = location + '#fnv1a64:' + item['id'].rsplit('#fnv1a64:', 1)[1]
                graph['models'].append(item)
                row['model_id'] = item['id']
                row['status'] = 'metadata-complete'
                if any(t['source_key'] is None for t in item['textures']) or any(
                        m['texture_reference_status'] != 'local-table' for m in item['materials']):
                    row['status'] = 'metadata-partial'
            elif suffix == '.bti':
                warnings = []
                key = texture_key(data, 0, standalone=True, warnings=warnings)
                row['textures'] = [{'index':0, 'name':Path(member).name, 'source_key':key,
                                    'provenance':'original-BTI-base-mip',
                                    'source_status':'available' if key else 'external-texture-unresolved'}]
                row['status'] = 'metadata-complete' if key else 'metadata-partial'
                if warnings:
                    row['warnings'] = warnings
            elif data[:4] == b'RARC':
                archive_record(location, raw, depth+1, 'member' if archive else 'file', data)
                row['status'] = 'container-expanded'
            elif suffix in ('.bmd', '.bdl', '.bmt'):
                row['status'] = 'metadata-unsupported'
                row['reason'] = 'unsupported model/material version'
        except (ValueError, struct.error) as error:
            row['status'] = 'invalid'
            row['reason'] = str(error)
            graph['failures'].append({'resource':location, 'reason':str(error)})
        return row

    paths = sorted(p for p in root.rglob('*') if p.is_file())
    if not paths:
        graph['failures'].append({'reason':'resource directory is empty'})
    for path in paths:
        relative = path.relative_to(root).as_posix()
        try:
            if path.stat().st_size > 512*1024*1024:
                raise ValueError('input exceeds 512 MiB')
            raw = path.read_bytes()
            if path.suffix.lower() == '.arc':
                archive_record(quote(relative, safe='/'), raw, 1, 'file')
            else:
                resource(None, relative, None, raw)
        except (ValueError, OSError, struct.error) as error:
            graph['failures'].append({'resource':relative, 'reason':str(error)})
    graph['models'].sort(key=lambda item:item['id'])
    graph['coverage'] = {
        'archives':len(graph['archives']), 'resources':len(graph['resources']),
        'input_files':len(paths), 'nested_archives':sum(a['source'] == 'member' for a in graph['archives']),
        'models':len(graph['models']), 'materials':sum(len(m['materials']) for m in graph['models']),
        'textures':sum(1 for _ in all_textures(graph)),
        'source_keys':sum(t['source_key'] is not None for t in all_textures(graph)),
        'unresolved_texture_sources':sum(t['source_key'] is None for t in all_textures(graph)),
        'palette_fallbacks':sum(len(r.get('warnings', [])) for r in graph['resources']) + sum(
            len(t.get('warnings', [])) for m in graph['models'] for t in m['textures']),
        'distinct_source_keys':len({tuple(sorted(t['source_key'].items())) for t in all_textures(graph)
                                   if t['source_key'] is not None}),
        'statuses':dict(sorted(collections.Counter(r['status'] for r in graph['resources']).items())),
        'formats':dict(sorted(collections.Counter(r['format'] for r in graph['resources']).items())),
        'inventory_complete':not graph['failures'],
        'supported_metadata_complete':not graph['failures'] and not any(
            r['status'] in ('metadata-partial','metadata-unsupported') for r in graph['resources']),
        'runtime_coverage':'not-measured'}
    return graph

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('archive',type=Path)
    parser.add_argument('--runtime-log',type=Path)
    parser.add_argument('--replacement-dir',type=Path)
    parser.add_argument('--midnafx-revision',required=True)
    parser.add_argument('--backend',default='offline')
    parser.add_argument('--platform',default='offline')
    parser.add_argument('--scene',default='not-observed')
    parser.add_argument('--output',type=Path,help='Write metadata JSON directly to this file')
    args=parser.parse_args()
    if args.output:
        output = args.output.resolve()
        source = args.archive.resolve()
        if output == source or (args.archive.is_dir() and output.is_relative_to(source)):
            parser.error('--output must be outside the input resource tree and must not replace the input archive')
        if args.replacement_dir and output.is_relative_to(args.replacement_dir.resolve()):
            parser.error('--output must be outside the replacement directory')
    graph={'schema':1,'midnafx_revision':args.midnafx_revision,
           'dusklight_revision':'edf42c6a7202647b56dd2fcdef02d17671bc814b',
           'backend':args.backend,'platform':args.platform,'scene':args.scene,'models':[], 'failures':[]}
    try:
        if args.archive.is_dir():
            if args.runtime_log:
                raise ValueError('resource-tree runtime join is ambiguous; use a single archive')
            context = {k:v for k,v in graph.items() if k not in ('schema','models','failures')}
            graph = census(args.archive)
            graph.update(context)
        else:
            for _,name,data in bmd.archive_entries(args.archive.read_bytes()):
                if Path(name).suffix.lower() not in ('.bmd','.bdl','.bmt'): continue
                try: graph['models'].append(model(bmd.yaz0(data),args.archive.name,name))
                except ValueError as error: graph['failures'].append({'resource':name,'reason':str(error)})
            if not graph['models'] and not graph['failures']:
                graph['failures'].append({'reason':'no supported model/material resources'})
        graph['models'].sort(key=lambda m:m['id'])
        if args.runtime_log: join_runtime(graph,args.runtime_log.read_text(errors='replace').splitlines())
        if args.replacement_dir: replacements(graph,args.replacement_dir)
    except (ValueError, OSError, struct.error, KeyError) as error:
        graph['failures'].append({'reason':str(error)})
    output = json.dumps(graph,sort_keys=True,indent=2) + '\n'
    if args.output:
        args.output.write_text(output, encoding='utf-8')
    else:
        print(output, end='')
    return bool(graph['failures'])

if __name__=='__main__':
    raise SystemExit(main())
