# Distinct material finishes — November 11, 2026

Native service 1.2 adds scoped per-draw finish parameters. The existing default-on
Material relighting control selects cached broad classifications. Fallback and
conflicting labels retain the previous shared response. No asset allowlist,
separate surface-map textures or new render pass is introduced.

## Windows D3D12 observations

- Ordon `F_SP103,0,27,0`, original textures, native time 180:
  [capture](ordon.png). Rock and wood draw requests, enhanced pipeline ready.
- Faron Spring `F_SP108,1,3,0`, supplied HenrikosTP4K_4.0d_4K pack enabled,
  native time 180: [capture](faron.png). Leather, cloth, metal, rock and wood draw
  requests; enhanced pipeline ready. Original material labels remain the
  classification source while replacement images supply the sampled detail.
- Forest Temple `D_MN05,3,0,0`, original textures: [capture](temple.png).
  Metal, wood and rock requests. Live module reload initializes a second time,
  rebuilds resident bindings, resumes metal/wood requests, and returns to the
  rendered [gameplay view](reloaded.png).

All three previews close normally with exit code 0. No logged WGSL/GPU validation
errors occur. Initial shader warmup briefly leaves some native pipelines pending;
Link and HUD render once compilation completes. These unfrozen captures establish
runtime context, not matched per-class pixel attribution or an artistic quality
score. Wet classification has a portable test but no native appearance receipt.

The clean trials log the existing missing `gczelda2` card file. The 4K pack also
logs rejected DDS entries with dimensions not aligned to compression blocks,
including UI images/mips; those images fall back. This is not proof that every
pack file loads. The Mods panel displays some corrupted text immediately after
reload; the subsequent gameplay view renders normally. That UI presentation
issue remains open and is not claimed to be caused or fixed by this feature.

## Identity and focused checks

New host SHA256:
`7FA566C403024F5F5C62CBAAD28A96EE7BD0D23529A3B6FD27B8CD81F3D2D779`.
Faron / Temple native package SHA256:
`C0CE24174FC45382F3F55BA5003AFE0279AF78D13E8F18F5D68B2E68347AF899`.
Final package SHA256:
`1351432C369E31F6D6AFF4A19BA1744E0BCDE9BC5C87803F7E775DD6016496EA`.
The final build adds archive-header and negative-directory-count guards to the
resident scan. Valid native materials use the same profile/shader path. All four
focused checks pass again after that build.
Earlier Ordon package SHA256:
`F0D92773AEDFC2F67C7B4D1F08FC0E5B0DEAE9B8A139FB3163ED37DC799E3418`.
The final classifier additionally recognizes generic apparel and hardware labels;
its rock/wood rules match the Ordon trial.

The host is promoted to `build/material-relighting/host-final/`; the previous
host remains in `host-before-finishes/` for rollback. Both API headers append the
1.2 callback after the 1.1 layout. Both source patches pass forward checking
against pinned Git indices and reverse checking against applied working files.

Four focused CTest entries pass: surface_class, native_material_compat,
presets_roundtrip and package_contract. Tests cover broad labels, conflicts,
unknowns, original labels with replacement-style filenames, legacy/truncated
capabilities, callback rejection and fallback restoration requests.

## Limits

This first pass caches semantic profiles and varies native-light highlights and
existing UV-derived relief. Separate authored/generated normal and roughness maps,
physical material reconstruction, opaque scenery reflections, measured GPU/CPU
cost, universal material coverage and macOS gameplay remain future/unverified.
Finish requests identify the CPU draw scope; they do not prove each material's
isolated visible pixel contribution. Native specular-only, blended, alpha-tested,
unlit and other unsupported paths retain their existing behavior. Cloth/leather
classification is a naming heuristic, not verified physical composition.
