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
