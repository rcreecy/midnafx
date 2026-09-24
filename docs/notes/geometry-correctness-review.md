# M7 read-only geometry catalog correctness review

Scope: the optional resource catalog in `src/game/geometry_probe.cpp`, not a
normal-mutation or smoothing implementation. Reviewed against pinned Dusklight
`edf42c6a7202647b56dd2fcdef02d17671bc814b` and the SDK hook contract.
There was no runtime game test.

## Findings resolved

1. **Hook installation could survive a failed post-callback registration.**
   `mods::hook::add_post` installs the detour before registering the callback
   (`sdk/include/mods/svc/hook.hpp:209–216`). Initially the probe marked the
   hook registered only when the whole call succeeded, so shutdown would skip
   uninstall after a registration failure. It now calls `uninstall` on failure.
2. **The catalog can omit already loaded models.** `loadResource` runs when an
   archive is loaded. With a default-off toggle, enabling it after reaching a
   scene cannot replay earlier loads. The panel now instructs the user to enable
   it before loading a scene and to reload for existing models. The investigation
   and runtime checklist state this limit.

## Contract checks

- `DEFINE_HOOK(&dRes_info_c::loadResource, ...)` maps a member-function receiver
  to callback argument zero; the post-callback receives an `int` return slot.
  The callback reads the object only after successful `loadResource` completion.
- The loader fills `mRes[fileIndex]` for processed resources before returning
  (`src/d/d_resorce.cpp:327–543`). The callback bounds file indices by
  `countFile`, checks file entries, and reads BMD model metadata while the
  archive is still alive. It stores no resource pointer after the callback.
- Config registration uses the existing persisted bool convention with a false
  default. The callback does no archive traversal or logging when disabled.
  The hook is removed at mod shutdown.
- The code uses the pinned SDK's game hook and J3D APIs shared by Windows and
  Intel macOS. A Windows compile verifies types; Intel macOS packaging remains
  a CI check, and runtime behavior remains untested.

No normal bytes, topology, draw lists, or GPU state are changed. Gate 1 remains
open until a named model passes the controlled mutation and unload/reload test;
Gate 2 and adaptive smoothing remain pending.

## Controlled mutation PoC review (September 2026)

The follow-up review covered the default-off metal-box-only mutation and its
archive lifecycle hooks against pinned Dusklight `edf42c6`. It found and
resolved these implementation defects:

1. The initial bounds check used `JKRArchive::getFileSize(entry)`, which can
   reflect an overlay file's size rather than the original buffer holding the
   normal array. The PoC now obtains `getExpandedResSize(model.getRawData())`,
   rejects its sentinel and out-of-range pointers, and checks the entire array.
2. Turning the diagnostic toggle off initially left the source normals changed
   until archive unload. It now restores them immediately through both the UI
   setter and the config subscription. A scene reload is still needed if an
   existing J3D instance copied or transformed those normals.
3. The offline Yaz0/RARC/BMD inspector now checks decompression references,
   archive bounds, and VTX1 offsets before reporting model metadata.

The reviewer found no further actionable C++ defect after these fixes. The
model is exact-allowlisted, F32 XYZ with stride 12, static and unweighted;
other models and unsupported formats are bypassed. The `loadResource` post-hook
observes completed J3D source data; the `deleteArchiveRes` pre-hook restores
bytes before teardown, and shutdown restores before uninstalling it. The
experiment has no per-frame path. No topology parser or smoothing algorithm
exists yet, so primitive winding, material boundaries, packed formats, and
skinning are Gate 2/design concerns rather than implemented behavior.

Windows compilation and an isolated source-matched Dusklight run reached the
exact `l_metabox_00.bmd` load and logged the upward-normal mutation once.
This is source-array mutation smoke evidence, not visual proof that TP renders
those changed normals. The timed smoke process did not exercise graceful
shutdown or restoration. Gate 1 remains open until original/mutated
screenshots and lifecycle checks on the named metal box are captured. Intel
macOS behavior remains runtime-unvalidated.

## Visible Gate 1 follow-up

The original upward-normal and subsequent negated-normal runs both reached the
target BMD. The negation keeps the same exact allowlist, F32 bounds, backup,
and restore lifecycle. Graceful exit from the visible negation run logged
restoration before mod unload. Matched captures of the visible metal surface
showed no unmistakable lighting difference; this is a failed visual proof,
not evidence that the mutation hook is generally ineffective. The precise
identity of the surface in view and which normal indices its shape references
remain unverified. Gate 1 stays open, and no topology or smoothing code was
added.

A second matched run at `F_SP116,3,13,2` loaded the target model near three
layer-2 `ironbox` actor placements. Nearby box-shaped objects still showed no
unmistakable change, and the rain made pixel comparisons less decisive.
The review therefore does not promote source-array mutation logs or a nearby
actor placement into a passed render-path gate.

The dedicated follow-up reviewer found a possible null dereference in a
temporary material-diagnostic log (`getColorChan(0)` may be null even when a
material exists). That diagnostic was removed after recording its observed
values; the shipped PoC does not dereference a color channel. No further
actionable defect was found in the negation and restore path.

## Subsequent Gate 1 evidence

This review recorded the state before a later controlled visibility test. That
test used a temporary render-only transform to bring the exact metal-box model
out from behind scenery. Original versus negated normals produced an
unmistakable lighting difference, documented in `docs/geometry-investigation.md`.
The temporary transform and draw hook were removed afterward. The earlier
"Gate 1 stays open" statements above describe the review checkpoint, not the
current binary renderer result. Lifecycle and broad-model checks remain open.

## Archive-owned identity-split integration review (September 2026)

The production default-off pot path now rebuilds its BMD before the J3D loader,
then applies the existing smoothing plan to the rebuilt normal array. The review
found and fixed two boundary defects: expanded resource size was queried before
checking the archive resource pointer, and the archive allocator size conversion
did not explicitly reject output larger than `u32`.

The final path is fail closed at each boundary: both loader and archive-delete
hooks must exist; the current heap must equal the resource archive heap; archive,
file name, source size, and source hash must match; the bounded transformer must
reparse successfully; allocation must succeed; and the loaded topology must
match the rebuilt fingerprint before normals change. A single optional record
prevents duplicate processing and avoids tracking allocations. The replacement
is archive-owned, while MidnaFX retains only non-owning pointers until the
archive pre-delete callback. Normal bytes are restored before that record is
cleared. Mod shutdown restores normal bytes before removing lifecycle hooks;
the archive-owned rebuilt data safely remains with any live model.

The pass checked malformed GX input, index and allocation bounds, zero-area
triangles, finite normal encoding, strip winding, material separation,
normal-index aliasing, shared instances, unload/reload, in-process disable, and
graceful shutdown. Runtime unload/reload used a new archive heap and rebuilt
exactly once per observed resource lifetime. Remaining risk is limited to the
broader model coverage and visual quality on the hidden skinned path; only the
exact rigid pot is user-facing, and the setting remains off by default.

The follow-up skinned checkpoint uses a separate hidden default-off switch and
an exact `Kmdl.arc/al.bmd` size/hash allowlist. Review verified that the same
bounded transformer, archive-heap ownership, rebuilt corner hash, S16 codec,
conflict-free plan, single active backup, disable callback, and shutdown restore
guard the write. The live model ran 3,600 frames and restored 60,384 bytes at
shutdown. No stale pointer or double mutation was observed.

Two limits remain deliberate. The implementation retains one replacement and
one active mutation, so the hidden Link experiment and pot experiment are not
supported concurrently. The dedicated activation review found one defect: the
resource hook's early-return condition omitted the hidden skinned switch, so it
only ran when an unrelated diagnostic or rigid-smoothing switch was enabled.
The condition now includes `geometry_skinned_smoothing_enabled()`. A live run
with both geometry diagnostics and topology diagnostics disabled rebuilt and
smoothed Link, used one instance, restored the normals, and unloaded cleanly.

A temporary host-only draw probe then confirmed the live GPU skinning path.
Link used flags `0x00080000`, no CPU skin flag, no `J3DSkinDeform`, and 134
normal matrices. Their hash changed at draws 30 and 120 while the rebuilt source
normal array remained the vertex buffer's current normal pointer. This proves
that existing animated GPU transforms consume the smoothed normals. The probe
was removed and the ordinary host rebuilt.

The final matched runtime pass used the same source-matched D3D11 host, save,
scene, camera, pose, resolution, and dialogue state for original and smoothed
`Kmdl.arc/al.bmd`. Evidence is retained locally under `build/m7-evidence/` as
`link-skinned-off.jpg`, `link-skinned-on.jpg`,
`link-skinned-comparison.jpg`, and their logs. The enabled log records 6,855
changed normals, zero index conflicts, one shared instance, and graceful
original-normal restoration. The comparison shows stable skinning, silhouette,
shield rim, clothing seams, and no exploding or inverted lighting. Link's
shading change is subtle in this pose, so broader art-quality review remains a
release risk. The hidden switch remains off by default.

No new correctness defect was found in this pass. The downloaded NTSC-U
practice save was used only as runtime test input. Temporary input bindings,
test save state, and both local and application geometry toggles were restored
after capture.
