"""Synthetic fixtures only: no game data required by CI."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import subprocess
import sys

spec=importlib.util.spec_from_file_location('asset',Path(__file__).parents[1]/'tools/asset-intelligence.py')
a=importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)

def fixture():
    mat=bytearray(0x220)
    mat[:4]=b'MAT3'
    struct.pack_into('>I',mat,4,len(mat))
    struct.pack_into('>H',mat,8,1)
    for off,value in [(12,0x80),(16,0x1d0),(20,0x1e0),(0x48,0x1d4)]:
        struct.pack_into('>I',mat,off,value)
    for s in range(8): struct.pack_into('>H',mat,0x80+0x84+s*2,0xffff if s else 0)
    struct.pack_into('>HHHH',mat,0x1e0,1,0,0,8)
    mat[0x1e8:0x1ea]=b'm\0'
    tex=bytearray(0x90)
    tex[:4]=b'TEX1'
    struct.pack_into('>I',tex,4,len(tex))
    struct.pack_into('>H',tex,8,1)
    struct.pack_into('>II',tex,12,0x20,0x40)
    tex[0x20]=14
    struct.pack_into('>HH',tex,0x22,8,8)
    struct.pack_into('>I',tex,0x3c,0x40)
    struct.pack_into('>HHHH',tex,0x40,1,0,0,8)
    tex[0x48:0x4a]=b't\0'
    tex[0x60:0x80]=bytes(range(32))
    data=bytearray(32)+mat+tex
    data[:8]=b'J3D2bmd3'
    struct.pack_into('>II',data,8,len(data),2)
    return bytes(data)

class AssetTests(unittest.TestCase):
    def test_hash_vectors(self):
        self.assertEqual(a.xxh64(b''),'ef46db3751d8e999')
        self.assertEqual(a.xxh64(b'a'),'d24ec4f1a98c6e5b')
        self.assertEqual(a.xxh64(b'abc'),'44bc2cf5ad770999')
        # Independent values from pinned upstream xxhash.h / XXH64(seed=0).
        for n, expected in [(31,'c346d2b59b4d8ee1'),(32,'cbf59c5116ff32b4'),
                            (33,'0c535d1acafb8ead'),(63,'e26aa9e2a95f8e4f'),
                            (64,'f7c67301db6713f0'),(127,'464d085810ce0199'),
                            (2048,'68534a48b7bf5f4d')]:
            self.assertEqual(a.xxh64(bytes(i&255 for i in range(n))),expected)

    def test_readonly_repeat_and_slots(self):
        data=fixture()
        before=bytes(data)
        one=a.model(data,'fixture.arc','fixture.bmd')
        self.assertEqual(one,a.model(data,'fixture.arc','fixture.bmd'))
        self.assertEqual(data,before)
        self.assertEqual(one['materials'][0]['texture_slots'],[0]+[65535]*7)
        self.assertEqual(one['textures'][0]['source_key']['gx_format'],14)

    def test_malformed(self):
        data=fixture()
        for n in (0,7,31,80,len(data)-1):
            with self.assertRaises(ValueError): a.model(data[:n],'x','y')
        bad=bytearray(data)
        struct.pack_into('>I',bad,32+12,0xffffffff)
        with self.assertRaises(ValueError): a.model(bad,'x','y')
        bad=bytearray(data)
        struct.pack_into('>H',bad,32+0x1d4,3)
        with self.assertRaises(ValueError): a.model(bad,'x','y')

    def test_palette_bounds(self):
        data=bytearray(fixture())
        tex=32+0x220
        data.extend(bytes(16))
        struct.pack_into('>I',data,8,len(data))
        struct.pack_into('>I',data,tex+4,0xa0)
        data[tex+0x20]=8
        struct.pack_into('>H',data,tex+0x20+10,16)
        struct.pack_into('>I',data,tex+0x20+12,0x60)
        data[tex+0x80:tex+0xa0]=bytes(range(32))
        key=a.model(data,'x','y')['textures'][0]['source_key']
        self.assertEqual(key['tlut_hash'],'cbf59c5116ff32b4')
        struct.pack_into('>H',data,tex+0x20+10,15)
        with self.assertRaises(ValueError): a.model(data,'x','y')

    def test_cli_failure_is_structured(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'invalid.arc'
            path.write_bytes(b'not an archive')
            result=subprocess.run([sys.executable,str(Path(a.__file__)),str(path),
                                   '--midnafx-revision','fixture'],capture_output=True,text=True)
        self.assertEqual(result.returncode,1)
        self.assertEqual(json.loads(result.stdout)['schema'],1)
        self.assertTrue(json.loads(result.stdout)['failures'])

    def test_instances_lifecycle_and_schema(self):
        item=a.model(fixture(),'x','y')
        graph={'models':[item]}
        def event(kind,**kw): return 'AssetGraph '+json.dumps(dict(schema=1,event=kind,id=item['id'],**kw))
        lines=[event('creation',ordinal=1),event('creation',ordinal=2),event('archive-delete'),
               event('material',material=0,texture_slots=[0]+[None]*7)]
        a.join_runtime(graph,lines)
        self.assertEqual(item['runtime_creations'],2)
        self.assertEqual(item['archive_delete_observations'],1)
        self.assertEqual(item['runtime_materials'],[0])
        with self.assertRaises(ValueError): a.join_runtime(graph,['AssetGraph {"schema":2}'])
        with self.assertRaises(ValueError): a.join_runtime(graph,[event('material',material=0,texture_slots=[1]+[None]*7)])
        with self.assertRaises(ValueError): a.join_runtime(graph,[event('material',material=-1,texture_slots=[0]+[None]*7)])
        a.join_runtime(graph,[event('unsupported',reason='event limit reached')])
        self.assertEqual(graph['runtime_limitations'],['event limit reached'])

    def test_replacement_candidates_do_not_change_original(self):
        item=a.model(fixture(),'x','y')
        original=copy.deepcopy(item)
        key=item['textures'][0]['source_key']
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/f"tex1_8x8_{key['texture_hash']}_14.png"
            path.write_bytes(b'filename fixture, not a decoded texture')
            a.replacements({'models':[item]},Path(root))
        self.assertEqual(item['id'],original['id'])
        self.assertEqual(item['textures'][0]['source_key'],key)
        self.assertEqual(len(item['textures'][0]['replacement_candidates']),1)
        self.assertIn('unavailable',item['textures'][0]['replacement_selection'])
        with self.assertRaises(ValueError): a.replacements({'models':[item]},Path(root))

    def test_recorded_live_graph_comparison(self):
        # Metadata-only runtime evidence; no game or replacement image bytes.
        root=Path(__file__).parents[1]/'docs/evidence/asset-intelligence-2026-10-19'
        graphs=[json.loads((root/f'pumpkin-graph-{suffix}.json').read_text()) for suffix in ('b','c','e')]
        models=[graph['models'][0] for graph in graphs]
        for graph in graphs:
            self.assertEqual(graph['failures'],[])
        stable=('id','archive','resource','sha256','bytes','materials')
        for item in models[1:]:
            self.assertEqual({k:item[k] for k in stable},{k:models[0][k] for k in stable})
            self.assertEqual([t['source_key'] for t in item['textures']],
                             [t['source_key'] for t in models[0]['textures']])
        self.assertEqual([m['runtime_creations'] for m in models],[24,12,12])
        self.assertEqual(models[0]['resource_load_observations'],2)
        self.assertEqual(models[0]['archive_delete_observations'],1)
        self.assertEqual(models[2]['runtime_materials'],[0,1])
        self.assertTrue(all(len(t['replacement_candidates'])==1 for t in models[2]['textures']))

    def test_draw_encoding_requires_matching_host_evidence(self):
        item=a.model(fixture(),'x','y')
        graph={'models':[item]}
        label={'schema':1,'event':'draw-label','id':item['id'],'material':0,'label':'MidnaFXAssetV1:12:1'}
        encoded={'schema':1,'event':'encoded','label':label['label'],'frame':2,'vertices':3,'indices':3,'instances':1}
        # Worker log order is not the binding contract; labels are copied tokens.
        a.join_runtime(graph,['AssetDraw '+json.dumps(encoded),'AssetGraph '+json.dumps(label)])
        self.assertEqual(item['runtime_encoded_draws'],1)
        self.assertEqual(item['runtime_encoded_materials'],[0])
        other=a.model(fixture(),'x','y')
        a.join_runtime({'models':[other]},['AssetGraph '+json.dumps(label)])
        self.assertEqual(other['rendered_object'],'not-observed')
        self.assertEqual(other['runtime_unencoded_labels'],1)
        with self.assertRaises(ValueError):
            a.join_runtime({'models':[other]},['AssetGraph '+json.dumps(label)]*2)
        encoded['instances']=0
        with self.assertRaises(ValueError): a.join_runtime(graph,['AssetDraw '+json.dumps(encoded)])
        for invalid in ([], None, dict(encoded,label=[]), dict(encoded,instances=True)):
            with self.assertRaises(ValueError):
                a.join_runtime(graph,['AssetDraw '+json.dumps(invalid)])
        with self.assertRaises(ValueError):
            a.join_runtime(graph,['AssetGraph '+json.dumps(dict(label,label=[]))])

    def test_recorded_draw_correlation(self):
        root=Path(__file__).parents[1]/'docs/evidence/asset-draw-2026-10-20'
        models=[]
        for suffix,count in [('f',64),('g',32),('h',0)]:
            graph=json.loads((root/f'pumpkin-graph-{suffix}.json').read_text())
            self.assertEqual(graph['failures'],[])
            item=graph['models'][0]
            models.append(item)
            self.assertEqual(item.get('runtime_draw_labels',0),count)
            self.assertEqual(item.get('runtime_encoded_draws',0),count)
            self.assertEqual(item.get('runtime_unencoded_labels',0),0)
            if count:
                self.assertEqual(item['runtime_encoded_materials'],[0,1])
            else:
                self.assertEqual(item['rendered_object'],'not-observed')
        for item in models[1:]:
            self.assertEqual(item['id'],models[0]['id'])
            self.assertEqual(item['materials'],models[0]['materials'])
            self.assertEqual([t['source_key'] for t in item['textures']],
                             [t['source_key'] for t in models[0]['textures']])

    def test_selected_slot_requires_bound_handle_evidence(self):
        item=a.model(fixture(),'x','y')
        label=dict(schema=1,event='draw-label',id=item['id'],material=0,label='MidnaFXAssetV1:30:1')
        encoded=dict(schema=1,event='encoded',label=label['label'],frame=1,vertices=3,indices=0,instances=1,
                     slot0=dict(bound=1,replacement=1,width=1024,height=1024,mips=11))
        lines=['AssetGraph '+json.dumps(label),'AssetDraw '+json.dumps(encoded)]
        a.join_runtime({'models':[item]},lines)
        self.assertEqual(item['runtime_slot0_bindings'],[dict(material=0,**encoded['slot0'])])
        self.assertIn('unavailable',item['textures'][0]['replacement_selection'])
        for slot in (None,{},dict(encoded['slot0'],bound=0),dict(encoded['slot0'],width=0),dict(encoded['slot0'],replacement=True)):
            with self.assertRaises(ValueError):
                a.join_runtime({'models':[a.model(fixture(),'x','y')]},['AssetDraw '+json.dumps(dict(encoded,slot0=slot))])

    def test_recorded_binding_transition(self):
        root=Path(__file__).parents[1]/'docs/evidence/asset-selection-2026-10-20'
        models=[]
        for suffix,count in [('i',32),('j',64),('k',0)]:
            graph=json.loads((root/f'pumpkin-graph-{suffix}.json').read_text())
            self.assertEqual(graph['failures'],[])
            item=graph['models'][0]
            models.append(item)
            self.assertEqual(item.get('runtime_encoded_draws',0),count)
        self.assertEqual([(s['material'],s['replacement'],s['width']) for s in models[0]['runtime_slot0_bindings']],
                         [(0,0,64),(1,0,64)])
        self.assertEqual([(s['material'],s['replacement'],s['width']) for s in models[1]['runtime_slot0_bindings']],
                         [(0,1,64),(0,1,2048),(1,1,64),(1,1,1024)])
        self.assertNotIn('runtime_slot0_bindings',models[2])
        self.assertEqual(models[1]['resource_load_observations'],2)
        self.assertEqual(models[1]['archive_delete_observations'],1)
        self.assertTrue(all(m['materials']==models[0]['materials'] for m in models))

if __name__=='__main__': unittest.main()
