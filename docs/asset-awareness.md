# Resource-tree asset awareness

MidnaFX can build an offline catalog of the supplied extracted resource tree in
one pass. This replaces repeated single-object discovery with a shared inventory
for models, materials, texture sources, animation files and other resources.
It requires no gameplay route, renderer fork, per-asset allowlist or live scan.
This is asset identity coverage, not comprehensive runtime material control.

## Use

From the repository root, with a legally available extracted game:

```powershell
python tools/asset-intelligence.py path/to/extracted/files `
  --midnafx-revision <tested-revision> `
  --replacement-dir path/to/texture_replacements `
  --output path/outside-the-resource-tree/catalog.json
```

`--replacement-dir` is optional. It reads filenames, not replacement image
payloads. The output must remain outside both input trees. Neither the game nor
the pack is modified or copied into the MidnaFX package. A resource tree produces
schema 2; the existing single-archive runtime join retains schema 1.
Use the whole extracted `files` directory to include audio, movies and other
non-rendering payloads as byte identities. A smaller `files/res` root is also
supported when only resource archives are needed.

## Coverage and identity

The catalog records every file in the supplied tree and every regular member of
valid RARC archives. It expands Yaz0 archives, compressed members and nested
archives, with a maximum nesting depth of eight. Unreachable directory nodes,
truncated tables, duplicate paths, invalid payload bounds and depth exhaustion
produce explicit failures rather than a successful partial inventory.

Archive paths, full member paths and file-table indices remain separate. Resource
IDs escape delimiters and percent signs, so shared basenames and nested containers
cannot merge identities. SHA-256 fingerprints identify original stored bytes.
Model records also retain the expanded original resource fingerprint used by the
existing runtime observer. Materials and textures retain their table indices;
shared images do not merge distinct materials or models.

The shared parser supports BMD2/MAT2, BMD3/MAT3, BDL3/4 and BMT2/3 material tables,
plus standalone BTI textures. It reads original material remaps, eight texture
slots, texture names where present, dimensions, GX formats, padded base-mip hashes
and palette hashes. Signed offsets follow the pinned native structures. Nullable
name tables preserve indices with `null` names. Missing external texture data
remains unresolved rather than hashing a header or guessing a replacement.

Palette range handling follows the pinned Aurora source-key implementation. If
padded indices exceed the declared palette, Aurora retains a zero TLUT hash. The
catalog records that key and a warning without reading beyond the declared
palette. A truncated declared palette still fails bounds validation.

Animation, layout, collision, particle and other unparsed formats retain byte
identity and format labels. Their semantic relationships are not inferred.
`identity-only`, `metadata-complete`, `metadata-partial`, `metadata-unsupported`,
`container-expanded` and `invalid` distinguish these outcomes. Inventory completion
and supported metadata completion have separate flags. Neither flag establishes
visibility, actor ownership, animation behavior or rendering control.

## Replacement compatibility and cost

Replacement candidates are indexed once by the original source key, including
dimensions, GX format and optional palette hash. Matching then costs one key
lookup per texture rather than scanning the full pack for every texture.
Candidate paths remain sorted, including duplicate providers. Wildcards, mod
priority, streaming state and the selected provider cannot be resolved offline.

The existing recorded provider join now accepts general exact-key DDS/PNG names
instead of two pumpkin filenames. It still requires matching draw labels,
registration and original slot-zero identity. This removes an exporter restriction;
it does not broaden the experimental host's pumpkin-only draw instrumentation.
Ambiguous duplicate runtime IDs and resource-tree runtime joins are rejected.

Archive expansion copies literal runs and overlapping back-references in blocks.
No new dependency is required. The full catalog is generated offline; gameplay
does not acquire a catalog scan, larger observer limits or rendering work.

## Runtime boundary

The opt-in runtime observer still retains at most 128 original BMD3 records and
4,096 events. Its capacity warning now appears when the first supported resource
is actually skipped, including truncation within the final archive. Filling the
buffer without skipping a resource does not falsely report saturation.

General resource/material initial bindings are available within those bounds.
Encoded draws, selected providers and reversible material changes remain qualified
only for the existing controlled targets. Complete live instance ownership,
animated binding history, generic draw attribution and material semantics remain
open. Increasing logging limits or guessing material classes would not close
those gaps safely.

Portable synthetic tests cover the supported format families, malformed inputs,
archive paths, nested compression, decoder overlap, unresolved references,
replacement matching and output safety. The supplied resource-tree result is
recorded in the [coverage receipt](evidence/asset-awareness-2026-11-02/README.md).
macOS remains best effort; these tools use Python's standard library, and no
Metal or macOS gameplay result is implied.
