"""Synthetic RARC path and Yaz0 expansion checks; no game data required."""
import importlib.util
from pathlib import Path
import random
import struct
import unittest

spec = importlib.util.spec_from_file_location('bmd', Path(__file__).parents[1] / 'tools/inspect-bmd.py')
bmd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bmd)


def archive(nodes):
    """Nodes contain (tag, [(name, child-node-or-payload), ...])."""
    node_base = 64
    file_base = node_base + 16 * len(nodes)
    file_count = sum(len(entries) for _, entries in nodes)
    string_base = file_base + 20 * file_count
    strings, offsets = bytearray(), {}
    for _, entries in nodes:
        for name, _ in entries:
            if name not in offsets:
                offsets[name] = len(strings)
                strings.extend(name.encode('latin-1') + b'\0')
    resource_base = string_base + len(strings)
    data = bytearray(resource_base)
    data[:4] = b'RARC'
    struct.pack_into('>II', data, 8, 32, resource_base - 32)
    struct.pack_into('>6I', data, 32, len(nodes), node_base - 32,
                     file_count, file_base - 32, len(strings), string_base - 32)
    data[string_base:resource_base] = strings
    index = 0
    for n, (tag, entries) in enumerate(nodes):
        at = node_base + n * 16
        data[at:at + 4] = tag.encode('ascii')
        struct.pack_into('>HI', data, at + 10, len(entries), index)
        for name, value in entries:
            entry = file_base + index * 20
            directory = isinstance(value, int)
            flags = 2 if directory else 0x11
            offset = value if directory else len(data) - resource_base
            struct.pack_into('>III', data, entry + 4, flags << 24 | offsets[name],
                             offset, 0 if directory else len(value))
            if not directory:
                data.extend(value)
            index += 1
    struct.pack_into('>I', data, 4, len(data))
    return bytes(data)


def tree():
    return [('ROOT', [('.', 0), ('..', 0xffffffff), ('left', 1), ('right', 2)]),
            ('BMDR', [('.', 1), ('..', 0), ('same.bmd', b'left')]),
            ('BMDR', [('.', 2), ('..', 0), ('same.bmd', b'right')])]


class ArchiveTests(unittest.TestCase):
    def test_paths_preserve_duplicate_basenames_and_legacy_view(self):
        data = archive(tree())
        self.assertEqual(list(bmd.archive_records(data)),
                         [('BMDR', 'left/same.bmd', 6, b'left'),
                          ('BMDR', 'right/same.bmd', 9, b'right')])
        self.assertEqual(list(bmd.archive_entries(data)),
                         [('BMDR', 'same.bmd', b'left'), ('BMDR', 'same.bmd', b'right')])

    def test_ranges_are_not_silently_clamped(self):
        data = bytearray(archive(tree()))
        struct.pack_into('>I', data, 64 + 12, 0xffffffff)
        with self.assertRaisesRegex(ValueError, 'entries exceed'):
            list(bmd.archive_records(data))

    def test_directory_cycle_shared_child_and_bad_index(self):
        for child in (0, 1, 999):
            nodes = tree()
            nodes[0][1][-1] = ('right', child)
            with self.assertRaisesRegex(ValueError, 'cyclic|child node'):
                list(bmd.archive_records(archive(nodes)))

    def test_orphan_node_and_entry_are_errors(self):
        nodes = tree() + [('VOID', [])]
        with self.assertRaisesRegex(ValueError, 'unreachable RARC node'):
            list(bmd.archive_records(archive(nodes)))
        data = bytearray(archive(tree()))
        struct.pack_into('>H', data, 64 + 2 * 16 + 10, 2)
        with self.assertRaisesRegex(ValueError, 'unreachable RARC file entry'):
            list(bmd.archive_records(data))

    def test_overlapping_node_entries(self):
        data = bytearray(archive(tree()))
        struct.pack_into('>I', data, 64 + 2 * 16 + 12, 4)
        with self.assertRaisesRegex(ValueError, 'multiple nodes'):
            list(bmd.archive_records(data))

    def test_bad_and_duplicate_names(self):
        for name in ('', '.', '..', 'bad/name', 'bad\\name'):
            with self.assertRaisesRegex(ValueError, 'invalid RARC'):
                list(bmd.archive_records(archive([('ROOT', [(name, b'x')])])))
        with self.assertRaisesRegex(ValueError, 'duplicate RARC member path'):
            list(bmd.archive_records(archive([('ROOT', [('same', b'a'), ('same', b'b')])])))

    def test_distinct_non_ascii_names_do_not_merge(self):
        records = list(bmd.archive_records(archive([('ROOT', [('\x80', b'a'), ('\x81', b'b')])])))
        self.assertEqual([r[1] for r in records], ['\x80', '\x81'])

    def test_truncated_payload_and_unterminated_name(self):
        data = bytearray(archive([('ROOT', [('x', b'payload')])]))
        struct.pack_into('>I', data, 80 + 12, 0xffffffff)
        with self.assertRaisesRegex(ValueError, 'payload exceeds'):
            list(bmd.archive_records(data))
        data = bytearray(archive([('ROOT', [('x', b'payload')])]))
        data[101] = ord('y')
        with self.assertRaisesRegex(ValueError, 'unterminated'):
            list(bmd.archive_records(data))

    def test_yaz0_literals(self):
        data = archive(tree())
        compressed = b'Yaz0' + struct.pack('>I', len(data)) + bytes(8)
        compressed += b''.join(b'\xff' + data[i:i + 8] for i in range(0, len(data), 8))
        self.assertEqual(bmd.yaz0(compressed), data)
        self.assertEqual(list(bmd.archive_records(compressed)), list(bmd.archive_records(data)))
        header = b'Yaz0' + struct.pack('>I', 3) + bytes(8)
        self.assertEqual(bmd.yaz0(header + b'\xffABC'), b'ABC')
        with self.assertRaisesRegex(ValueError, 'truncated Yaz0 literal'):
            bmd.yaz0(header + b'\xffAB')

    def test_yaz0_overlap_and_nonoverlap(self):
        header = lambda size: b'Yaz0' + struct.pack('>I', size) + bytes(8)
        self.assertEqual(bmd.yaz0(header(7) + b'\x80A\x40\x00'), b'AAAAAAA')
        self.assertEqual(bmd.yaz0(header(9) + b'\xe0ABC\x40\x02'), b'ABCABCABC')
        self.assertEqual(bmd.yaz0(header(6) + b'\xe0ABC\x10\x02'), b'ABCABC')
        self.assertEqual(bmd.yaz0(header(21) + b'\x80A\x00\x00\x02'), b'A' * 21)

    def test_yaz0_invalid_reference_and_output_overrun(self):
        header = lambda size: b'Yaz0' + struct.pack('>I', size) + bytes(8)
        for data in (header(3) + b'\x00\x10\x00', header(3) + b'\x80A\x40\x00'):
            with self.assertRaisesRegex(ValueError, 'invalid Yaz0 back-reference'):
                bmd.yaz0(data)

    def test_yaz0_mixed_streams_match_sequential_copy(self):
        generator = random.Random(47)
        for _ in range(100):
            decoded, encoded = bytearray(), bytearray()
            for _ in range(8):
                control, tokens = 0, bytearray()
                for bit in range(8):
                    if not decoded or generator.randrange(3) == 0:
                        control |= 0x80 >> bit
                        value = generator.randrange(256)
                        tokens.append(value)
                        decoded.append(value)
                    else:
                        distance = generator.randrange(1, min(len(decoded), 4096) + 1)
                        count = generator.randrange(3, 274)
                        first = (distance - 1) >> 8
                        if count < 18:
                            first |= (count - 2) << 4
                        tokens.extend((first, (distance - 1) & 255))
                        if count >= 18:
                            tokens.append(count - 18)
                        for _ in range(count):
                            decoded.append(decoded[-distance])
                encoded.append(control)
                encoded.extend(tokens)
            compressed = b'Yaz0' + struct.pack('>I', len(decoded)) + bytes(8) + encoded
            self.assertEqual(bmd.yaz0(compressed), decoded)


if __name__ == '__main__':
    unittest.main()
