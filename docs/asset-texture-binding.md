# Sampled pumpkin texture binding

This extension to the [draw correlation experiment](asset-draw-correlation.md) observes slot 0 of the actual native texture bind group. It records whether the slot has a texture, the resolved handle's `isReplacement` flag, dimensions and mip count. The values are copied when the bind group is built, cached with that bind group, copied into the frame command, and logged only after successful native WebGPU draw encoding.

This establishes resolved replacement status for the sampled binding. It does not identify a provider, registration generation, filename, full-resolution streaming tier, or GPU completion. The original MAT3/TEX1 identity remains separate. A replacement handle can contain a streaming thumbnail; dimensions must be interpreted as the bound handle's dimensions rather than the source file's advertised size.

The existing experimental host patch contains this extension. Both diagnostics remain disabled by default in the mod. Normal package builds do not apply the host patch. Slot 0 is the only observed slot; other materials and animated texture references are outside this proof.

## Ownership and cost

The snapshot contains values only, with no borrowed resource or instance pointer. An eight-byte binding snapshot uses bounded integer fields; dimensions or mip counts exceeding their representation produce an unsupported marker that the offline parser rejects. Unbound slots contain zero metadata. Bind-group cache reuse preserves the snapshot for the exact bound texture view. Original and indirect shader sampling flags determine whether slot 0 is actually bound.

The existing 128-byte draw payload has no room for this extension. The diagnostic host increases inline payload capacity to 160 bytes, adding 32 bytes per inline payload and potentially increasing aligned command/storage sizes. This applies to the experimental host even when mod diagnostics are disabled. Debug-group copying and traced batch splitting from the earlier patch also remain. Complete host CPU/allocation overhead and experimental Metal-host compilation remain unverified; this is not a production performance claim.

The offline parser accepts older records without slot metadata, validates new records, and exports deduplicated `runtime_slot0_bindings` by material. Missing metadata remains unobserved. Existing `replacement_selection` fields still describe the absent public TextureService query for exact provider/file identity.

## Live validation

Windows D3D12 sessions use the unchanged mod package SHA-256 `7443ef6468082f48ee3311aadb490518e54ecafbe72da20bb2398338667a9287`. The [summary](evidence/asset-selection-2026-10-20/summary.json) records host, patch and full-log hashes. Metadata-only graphs and events are committed; full logs and captures remain under `build/m13-aa-runtime/asset-selection-*`.

- [Vanilla i](evidence/asset-selection-2026-10-20/pumpkin-graph-i.json): 32 labels and 32 encoded draws. Both materials bind original 64×64 textures with one mip.
- [Pack j](evidence/asset-selection-2026-10-20/pumpkin-graph-j.json): 64 labels and 64 encoded draws over two Ordon loads. Initially both materials bind 64×64 replacement handles with seven mips. After leaving for `F_SP127` and returning to Ordon, material 0 binds a 2048×2048 replacement with 12 mips; material 1 binds a 1024×1024 replacement with 11 mips. One archive deletion and two resource loads are observed.
- [Disabled k](evidence/asset-selection-2026-10-20/pumpkin-graph-k.json): zero draw labels and zero encoded records, while identity diagnostics remain active.

Every session reaches the rendered scene and shuts down normally. The original material mappings and source identity remain unchanged. The initial replacement dimensions agree with the pinned host's thumbnail behavior; the observed later dimensions agree with the two candidate DDS headers. Neither agreement proves exact provider/file selection.

The experimental host build passes after increasing payload capacity. Windows CTest passes **18/18** in **48.18 seconds**. The final Python suite contains **12 tests**, including malformed binding records and recorded streaming transitions. No new native macOS host or GPU-completion claim follows from these Windows sessions.

## Reproduction

Use the same isolated launcher and Ordon scene as the prior draw experiment. Apply the version-pinned patch with `tools/asset-draw-host-patch.py --apply`, rebuild the host, enable both diagnostic settings, and compare vanilla, pack-enabled and draw-disabled sessions. Close the host before exporting graphs. The mod package itself is unchanged.
