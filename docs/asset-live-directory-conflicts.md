# Live duplicate replacement files and fallback

The Windows D3D12 experiment confirms how the pinned host handles duplicate source keys within one replacement-directory load. It selects the first candidate in sorted relative-path order and registers that key once. This occurs before the [registry priority rules](asset-replacement-conflicts.md): the loader's `registeredKeys` set skips later duplicate files. Equal-priority registry entries still use the latest registration, but this directory loader does not create those competing entries.

## Experiment and result

Two original Henriko pumpkin DDS files were copied into an isolated root under both `A-first` and `z-second`. Each copy's SHA-256 matches the existing [pack manifest](evidence/asset-intelligence-2026-10-19/pack-manifest.json). The duplicate files deliberately contain identical image bytes. File selection is established by copied draw registration metadata and matching path fingerprints, not visual differences.

| Session | Files in isolated root | Host registrations | Sampled result |
|---|---:|---:|---|
| [o: duplicates](evidence/asset-live-conflicts-2026-10-20/summary-o.json) | 4 | 2 | Both materials select `A-first` |
| [p: first provider removed](evidence/asset-live-conflicts-2026-10-20/summary-p.json) | 2 | 2 | Both materials select `z-second` |
| [q: all providers removed](evidence/asset-live-conflicts-2026-10-20/summary-q.json) | 0 | 0 | Both materials bind original 64×64 textures with registration zero |

Each session matches **32/32** sampled labels to native encoded draws and observes both materials. All render normally and shut down normally. Original resource ID, SHA-256, material mappings and texture source keys remain unchanged. Texture replacements and both diagnostic switches remain enabled in all three sessions; the last run uses an empty isolated replacement root.

The host is closed between sessions. Removing a provider means moving only its copied directory outside the loaded root before the next launch. This proves fallback on a fresh directory load. It does not prove live hot-unregistration, conflicting direct registry providers in game, GPU completion, pixel equivalence, or all pack combinations. The original pack and installed user settings are not modified.

Registration IDs 1 and 2 occur in both replacement sessions. They are process-local values; they cannot identify a provider across launches. The basename and path fingerprint distinguish the selected directory within each session. FNV-1a path fingerprints are non-cryptographic and are checked against every observed candidate path; none collide in this experiment. File-content hashes attest the local copies, not GPU execution.

## Evidence and verification

The [evidence directory](evidence/asset-live-conflicts-2026-10-20/) contains per-session summaries, joined graphs and raw metadata event subsets. Summaries record every candidate's relative path, byte count, SHA-256 and logical host-path fingerprint, selected files, host/package hashes and full stdout hashes. Candidate snapshots were captured before the next directory move. Full local logs and screenshots remain under `build/m13-aa-runtime/asset-live-conflict-o`, `-p` and `-q`.

The unchanged mod package has SHA-256 `7443ef6468082f48ee3311aadb490518e54ecafbe72da20bb2398338667a9287`; the unchanged experimental host has SHA-256 `482aa1fcc8c4789297ba3b4d8476582a8e1868c33be578a982628ca35e8f3e8b`. This milestone adds no host, mod, or rendering behavior. No image payload is committed.

The Python asset suite passes **15/15**. The added regression case recomputes logical candidate-path fingerprints, checks directory selection and original identity, and re-runs the runtime join from raw event subsets. It requires no game or pack in CI.

Local Windows CTest passes **18/18** in 41.25 seconds. All three saved settings snapshots retain replacement support, identity diagnostics and draw diagnostics enabled. Defaults and installed user settings remain unchanged.

## Reproduction and remaining work

Use the pinned experimental host and existing diagnostic package. Create a fresh isolated replacement root with `A-first` and `z-second` subdirectories. Copy `tex1_64x64_000e342106676e31_14.dds` and `tex1_64x64_630d5c7bc606585d_14.dds` from the verified pack into both directories. Load Ordon `F_SP103,0,27,0` with both asset settings and replacements enabled, using isolated user/mod directories.

Close the host and export the graph with `tools/asset-intelligence.py`, supplying `--runtime-log` and the current `--replacement-dir`. Record the candidate snapshot before moving anything. Move `A-first` outside the loaded root, launch with a fresh isolated user directory, and repeat. Finally move `z-second` outside the loaded root and repeat with replacement support still enabled. Match provider path fingerprints against the logical user-directory path, including the `texture_replacements` junction path rather than its physical target.

Total observer CPU/allocation overhead and experimental Metal-host qualification remain open. The next compatibility gate is live removal/re-registration while frames remain in flight. No broader renderer feature or release promotion follows from this directory-load proof.
