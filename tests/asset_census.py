"""Synthetic whole-tree asset coverage and metadata fallback checks."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from asset_intelligence import a, fixture
from archive_inventory import archive


def literals(data):
    return (b'Yaz0' + struct.pack('>I', len(data)) + bytes(8) +
            b''.join(b'\xff' + data[i:i+8] for i in range(0, len(data), 8)))


def bti():
    data = bytearray(64)
    data[0] = 14
    struct.pack_into('>HH', data, 2, 8, 8)
    data[32:] = bytes(range(32))
    return bytes(data)


def material_only():
    data = bytearray(fixture()[:32+0x220])
    data[:8] = b'J3D2bmt3'
    struct.pack_into('>II', data, 8, len(data), 1)
    return bytes(data)


def legacy_fixture(material=False, include_texture=True):
    mat = bytearray(0x350)
    mat[:4] = b'MAT2'
    struct.pack_into('>I', mat, 4, len(mat))
    struct.pack_into('>H', mat, 8, 1)
    for offset, value in ((12, 0x80), (16, 0x300), (20, 0), (0x3c, 0x310), (0x48, 0xffffffff)):
        struct.pack_into('>I', mat, offset, value)
    struct.pack_into('>H', mat, 0x300, 1)  # Remap to the second init record.
    struct.pack_into('>HH', mat, 0x310, 65535, 0)
    for record in range(2):
        for slot in range(8):
            ref = 1 if record == 1 and slot == 0 else 65535
            struct.pack_into('>H', mat, 0x80+record*0x138+0x70+slot*2, ref)
    tex = fixture()[32+0x220:] if include_texture else b''
    data = bytearray(32)+mat+tex
    data[:8] = b'J3D2bmt2' if material else b'J3D2bmd2'
    struct.pack_into('>II', data, 8, len(data), 2 if include_texture else 1)
    return bytes(data)


class CensusTests(unittest.TestCase):
    def test_tree_preserves_archive_and_member_paths(self):
        nodes = [('ROOT', [('left', 1), ('right', 2)]),
                 ('BMDR', [('same.bmd', fixture())]),
                 ('BMDR', [('same.bmd', fixture())])]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for directory in ('one', 'two'):
                (root/directory).mkdir()
                (root/directory/'same.arc').write_bytes(archive(nodes))
            first = a.census(root)
            second = a.census(root)
        self.assertEqual(first, second)
        self.assertEqual(first['failures'], [])
        self.assertEqual(first['coverage']['archives'], 2)
        self.assertEqual(first['coverage']['models'], 4)
        self.assertTrue(first['coverage']['inventory_complete'])
        self.assertEqual(len({m['id'] for m in first['models']}), 4)
        self.assertEqual({r['member'] for r in first['resources']},
                         {'left/same.bmd', 'right/same.bmd'})
        self.assertEqual({r['archive'] for r in first['resources']},
                         {'one/same.arc', 'two/same.arc'})

    def test_shared_material_parser_supports_model_families(self):
        baseline = a.model(fixture(), 'x.arc', 'x.bmd')
        for magic in (b'J3D2bdl3', b'J3D2bdl4', b'J3D2bmt3'):
            data = bytearray(fixture())
            data[:8] = magic
            parsed = a.model(data, 'x.arc', 'x.bdl')
            self.assertEqual(parsed['format'], magic.decode('ascii'))
            self.assertEqual(parsed['materials'], baseline['materials'])
            self.assertEqual(parsed['textures'], baseline['textures'])

    def test_material_without_texture_table_stays_unresolved(self):
        parsed = a.model(material_only(), 'x.arc', 'x.bmt')
        self.assertEqual(parsed['textures'], [])
        self.assertEqual(parsed['materials'][0]['texture_slots'], [0]+[65535]*7)
        self.assertEqual(parsed['materials'][0]['texture_reference_status'],
                         'external-table-unresolved')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'x.bmt').write_bytes(material_only())
            graph = a.census(root)
        self.assertEqual(graph['resources'][0]['status'], 'metadata-partial')
        self.assertTrue(graph['coverage']['inventory_complete'])
        self.assertFalse(graph['coverage']['supported_metadata_complete'])

    def test_legacy_material_stride_remap_and_texture_table(self):
        for material in (False, True):
            parsed = a.model(legacy_fixture(material), 'x.arc', 'legacy')
            self.assertEqual(parsed['materials'][0]['texture_slots'], [0]+[65535]*7)
            self.assertEqual(parsed['materials'][0]['provenance'], 'original-MAT2-remap')
            self.assertIsNotNone(parsed['textures'][0]['source_key'])
        parsed = a.model(legacy_fixture(material=True, include_texture=False), 'x.arc', 'legacy.bmt')
        self.assertEqual(parsed['textures'], [])
        self.assertEqual(parsed['materials'][0]['texture_slots'], [0]+[65535]*7)
        self.assertEqual(parsed['materials'][0]['texture_reference_status'], 'external-table-unresolved')

    def test_standalone_zero_offset_and_tex1_placeholder_differ(self):
        key = a.texture_key(bti(), 0, standalone=True)
        self.assertEqual(key['texture_hash'], a.xxh64(bytes(range(32))))
        self.assertIsNone(a.texture_key(bti(), 0))
        data = bytearray(fixture())
        struct.pack_into('>I', data, 32+0x220+0x20+28, 0)
        parsed = a.model(data, 'x', 'y')
        self.assertIsNone(parsed['textures'][0]['source_key'])
        self.assertEqual(parsed['textures'][0]['source_status'], 'external-texture-unresolved')

    def test_signed_texture_offsets_and_bounds(self):
        data = bytearray(bytes(range(32)) + bti()[:32])
        struct.pack_into('>i', data, 32+28, -32)
        self.assertEqual(a.texture_key(data, 32)['texture_hash'], a.xxh64(bytes(range(32))))
        struct.pack_into('>i', data, 32+28, -33)
        with self.assertRaises(ValueError):
            a.texture_key(data, 32)
        # Both palette and pixel pointers may precede their individual header.
        data = bytearray(bytes(range(32)) + bytes(32) + bti()[:32])
        data[64] = 8
        struct.pack_into('>H', data, 64+10, 16)
        struct.pack_into('>i', data, 64+12, -64)
        struct.pack_into('>i', data, 64+28, -32)
        key = a.texture_key(data, 64)
        self.assertEqual(key['texture_hash'], a.xxh64(bytes(32)))
        self.assertEqual(key['tlut_hash'], a.xxh64(bytes(range(2))))

    def test_missing_names_preserve_indices(self):
        data = bytearray(fixture())
        struct.pack_into('>I', data, 32+20, 0)
        struct.pack_into('>I', data, 32+0x220+16, 0)
        parsed = a.model(data, 'x', 'y')
        self.assertIsNone(parsed['materials'][0]['name'])
        self.assertIsNone(parsed['textures'][0]['name'])
        self.assertEqual(parsed['materials'][0]['index'], 0)
        self.assertIsNotNone(parsed['textures'][0]['source_key'])

    def test_compressed_members_and_loose_textures(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            raw = archive([('BMDR', [('model.bmd', literals(fixture()))])])
            (root/'compressed.arc').write_bytes(literals(raw))
            (root/'texture.bti').write_bytes(literals(bti()))
            (root/'motion.bck').write_bytes(b'J3D1bck1'+bytes(24))
            graph = a.census(root)
        self.assertEqual(graph['failures'], [])
        self.assertEqual(graph['coverage']['resources'], 3)
        self.assertEqual(graph['coverage']['textures'], 2)
        self.assertEqual(graph['coverage']['distinct_source_keys'], 1)
        self.assertEqual(graph['models'][0]['sha256'], hashlib.sha256(fixture()).hexdigest())
        self.assertEqual([r['encoding'] for r in graph['resources'] if r['id'].endswith('.bti')], ['Yaz0'])
        self.assertEqual([r['status'] for r in graph['resources'] if r['member']=='motion.bck'], ['identity-only'])

    def test_nested_compressed_archives_expand_each_level(self):
        deepest = archive([('TEX ', [('texture.bti', bti())])])
        middle = archive([('ROOT', [('deep.arc', literals(deepest))])])
        outer = archive([('ROOT', [('middle.arc', literals(middle))])])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'outer.arc').write_bytes(literals(outer))
            graph = a.census(root)
        self.assertEqual(graph['failures'], [])
        self.assertEqual(graph['coverage']['archives'], 3)
        self.assertEqual(graph['coverage']['nested_archives'], 2)
        self.assertEqual(graph['coverage']['resources'], 3)
        self.assertEqual(graph['coverage']['textures'], 1)
        self.assertEqual(graph['resources'][-1]['id'], 'outer.arc!middle.arc!deep.arc!texture.bti')
        self.assertEqual({entry['depth'] for entry in graph['archives']}, {1, 2, 3})

    def test_nested_depth_limit_is_explicit(self):
        payload = archive([('TEX ', [('texture.bti', bti())])])
        for depth in range(8):
            payload = archive([('ROOT', [(f'level{depth}.arc', payload)])])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'root.arc').write_bytes(payload)
            graph = a.census(root)
        self.assertEqual(graph['coverage']['archives'], 9)
        self.assertFalse(graph['coverage']['inventory_complete'])
        self.assertEqual(graph['archives'][-1]['status'], 'incomplete')
        self.assertEqual(graph['archives'][-1]['depth'], 9)
        self.assertEqual(len(graph['failures']), 1)
        self.assertIn('depth exceeds 8', graph['failures'][0]['reason'])

    def test_resource_identity_escapes_archive_delimiter(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'a.arc').write_bytes(archive([('TEX ', [('x.bti', bti())])]))
            (root/'a.arc!x.bti').write_bytes(bti())
            (root/'a.arc%21x.bti').write_bytes(bti())
            graph = a.census(root)
        self.assertEqual(graph['failures'], [])
        self.assertEqual({row['id'] for row in graph['resources']},
                         {'a.arc!x.bti', 'a.arc%21x.bti', 'a.arc%2521x.bti'})

    def test_palette_range_fallback_never_reads_beyond_declared_palette(self):
        data = bytearray(bti()+bytes(2))
        data[0] = 8
        struct.pack_into('>H', data, 10, 1)
        struct.pack_into('>I', data, 12, 64)
        struct.pack_into('>I', data, 28, 32)
        data[32:64] = b'\xff'*32
        warnings = []
        key = a.texture_key(data, 0, standalone=True, warnings=warnings)
        self.assertEqual(key['tlut_hash'], '0000000000000000')
        self.assertEqual(key['texture_hash'], a.xxh64(b'\xff'*32))
        self.assertEqual(warnings, ['palette range exceeds declared colors; native zero TLUT hash'])
        with self.assertRaisesRegex(ValueError, 'bounds'):
            a.texture_key(data[:-1], 0, standalone=True, warnings=[])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'fallback.bti').write_bytes(data)
            graph = a.census(root)
        self.assertEqual(graph['failures'], [])
        self.assertEqual(graph['resources'][0]['warnings'], warnings)

    def test_bad_member_does_not_hide_following_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'a.arc').write_bytes(archive([('BMDR', [('bad.bmd', fixture()[:-1])])]))
            (root/'b.arc').write_bytes(archive([('BMDR', [('good.bmd', fixture())])]))
            (root/'unknown.bmd').write_bytes(b'J3D2bmd1'+bytes(24))
            graph = a.census(root)
        self.assertEqual(graph['coverage']['archives'], 2)
        self.assertEqual(graph['coverage']['resources'], 3)
        self.assertEqual(graph['coverage']['models'], 1)
        self.assertFalse(graph['coverage']['inventory_complete'])
        self.assertEqual(len(graph['failures']), 1)
        self.assertEqual(graph['failures'][0]['resource'], 'a.arc!bad.bmd')
        self.assertEqual(graph['resources'][-1]['status'], 'metadata-unsupported')

    def test_truncated_compressed_member_keeps_structured_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            truncated = b'Yaz0'+struct.pack('>I', 100)+bytes(8)+b'\xff'
            (root/'broken.arc').write_bytes(archive([('BMDR', [('broken.bmd', truncated)])]))
            graph = a.census(root)
        self.assertEqual(graph['resources'][0]['status'], 'invalid')
        self.assertFalse(graph['coverage']['inventory_complete'])
        self.assertEqual(len(graph['failures']), 1)
        self.assertIn('truncated Yaz0', graph['failures'][0]['reason'])

    def test_replacement_index_retains_exact_candidates(self):
        parsed = a.model(fixture(), 'x', 'y')
        key = parsed['textures'][0]['source_key']
        filename = f"tex1_8x8_{key['texture_hash']}_14.dds"
        palette_key = dict(key, tlut_hash='1234567890abcdef')
        graph = {'models':[parsed], 'resources':[{'textures':[
            {'source_key':palette_key}, {'source_key':None}]}]}
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for directory in ('a', 'b'):
                (root/directory).mkdir()
                (root/directory/filename).write_bytes(b'candidate')
            (root/filename.replace('8x8', '16x8')).write_bytes(b'wrong dimensions')
            palette = filename.replace('_14.dds', '_1234567890abcdef_14.PNG')
            (root/palette).write_bytes(b'palette candidate')
            a.replacements(graph, root)
        self.assertEqual(parsed['textures'][0]['replacement_candidates'], ['a/'+filename, 'b/'+filename])
        self.assertEqual(graph['resources'][0]['textures'][0]['replacement_candidates'], [palette])
        self.assertEqual(graph['resources'][0]['textures'][1]['replacement_candidates'], [])

    def test_duplicate_runtime_identity_rejected(self):
        parsed = a.model(fixture(), 'x', 'y')
        with self.assertRaisesRegex(ValueError, 'ambiguous duplicate'):
            a.join_runtime({'models':[parsed, copy.deepcopy(parsed)]}, [])

    def test_provider_join_accepts_general_exact_key(self):
        parsed = a.model(fixture(), 'x', 'y')
        key = parsed['textures'][0]['source_key']
        label = dict(schema=1, event='draw-label', id=parsed['id'], material=0, label='MidnaFXAssetV1:9:1')
        encoded = dict(schema=1, event='encoded', label=label['label'], frame=1, vertices=3, indices=0,
                       instances=1, slot0=dict(bound=1, replacement=1, width=1024, height=1024,
                                               mips=11, registration=5))
        provider = dict(schema=1, event='handle-created', registration=5,
                        file=f"tex1_8x8_{key['texture_hash']}_14.png", path_fnv1a64='0'*16)
        a.join_runtime({'models':[parsed]}, ['AssetGraph '+json.dumps(label),
                       'AssetDraw '+json.dumps(encoded), 'AssetProvider '+json.dumps(provider)])
        self.assertEqual(parsed['runtime_slot0_bindings'][0]['provider']['file'], provider['file'])

    def test_cli_directory_output_and_runtime_refusal(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)/'assets'
            root.mkdir()
            (root/'x.bti').write_bytes(bti())
            output = Path(temporary)/'catalog.json'
            command = [sys.executable, str(Path(a.__file__)), str(root),
                       '--midnafx-revision', 'fixture', '--output', str(output)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, '')
            graph = json.loads(output.read_text(encoding='utf-8'))
            self.assertEqual(graph['schema'], 2)
            self.assertEqual(graph['coverage']['resources'], 1)
            self.assertEqual(graph['midnafx_revision'], 'fixture')
            result = subprocess.run(command+['--runtime-log', str(root/'absent.log')],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn('ambiguous', json.loads(output.read_text(encoding='utf-8'))['failures'][0]['reason'])

    def test_cli_output_cannot_replace_input_or_enter_resource_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root/'source.arc'
            raw = archive([('BMDR', [('model.bmd', fixture())])])
            source.write_bytes(raw)
            command = [sys.executable, str(Path(a.__file__)), str(source),
                       '--midnafx-revision', 'fixture', '--output', str(source)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('must not replace the input archive', result.stderr)
            self.assertEqual(source.read_bytes(), raw)
            output = root/'catalog.json'
            command[2] = str(root)
            command[-1] = str(output)
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('outside the input resource tree', result.stderr)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
