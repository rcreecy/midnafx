# Native menu root-focus repair (2026-10-09)

F1 failed to open Dusklight's native menu in isolated stage launches. Temporary
host logging proved that SDL delivered F1 (`key=1073741882`, `scancode=58`),
the debug UI did not capture it, and RmlUi mapped it to key 107. Focus was on
the context root (`#root`, id `main`), with four documents loaded. Keyboard
events targeted that root rather than a document, so the document listeners
that implement menu navigation never received them.

`patches/dusklight-menu-root-focus.patch` adds a scoped listener owned by MenuBar
to the context root. It handles only a Menu command targeted directly at that
root, only while MenuBar is the top active document, and delegates to the
existing navigation handler. It does not capture child-document events, change
key mappings, or bypass an active modal/console. Listener lifetime follows the
existing Document listener ownership. No rendering or AA code changes.

The patch is applied to the local test host, alongside the existing camera
patch. It is not included in the released MidnaFX package or a published host.
Reverse-apply validation passes against the current checkout. All temporary
Aurora input logging was removed, its original bytes restored, and its source
timestamp advanced before rebuilding to avoid stale instrumented objects.

Runtime evidence lives in `build/m13-aa-runtime/release120-menu-fixed`:

- F1 opens the native menu from gameplay, dismisses it, and opens it again.
- The Mods panel shows the published MidnaFX v1.2.0 package active.
- With no saved AA key, the Basic AA control initially shows On.
- Clicking AA changes it to Off and back to On in the same session.
- Native Reload logs deactivation, reactivation, and one active mod; AA stays On.
- Gameplay renders after menu dismissal with an intact HUD, then unloads cleanly.

Screenshots cover these states. They are lifecycle evidence, not a matched
pixel comparison or temporal-quality evaluation. The fresh isolated user
directory still has no save or assigned input device. Installed user settings
and the released package are unchanged.

## Follow-up live check

`release120-live-validation` confirms the Basic controls show grading On and
AA On. Attempts to enter a nonzero exposure did not change the saved value
from zero, so this run does not validate grading bypass with a non-neutral look.

The native Warp action requested Hyrule Field, room 0, point 0, layer -1.
Logs show F_SP121 resources loaded, but the gameplay view remained black on
subsequent captures. F1 still opened and dismissed the responsive Warp panel.
This is an unresolved transition result, not a passed AA lifecycle check or
evidence that AA caused the black view. `warp-black.png` records the outcome.
The host closed through its window control and logged all mods unloaded.
Teardown emitted the existing buffer-mapping abort and device-destruction
warnings. The saved configuration still has AA enabled, grading enabled,
and exposure zero. No test host remains running.

## Controlled exposure verification

A new isolated run, `release120-exposure-control`, starts the published package
with exposure 100 (+1 EV), grading enabled, and AA enabled. The scene is visibly
brighter. Disabling grading through the Basic control removes that brightness
while the AA control remains On. After closing the panel, gameplay and HUD render
normally. Screenshots record the bright scene, both toggle states, and the
grading-disabled scene. The saved configuration confirms exposure 100 is
preserved, grading is false, and AA is true. This closes the non-neutral exposure
bypass smoke check, but does not establish pixel identity or Twilight blending
behavior. The host unloads the mod cleanly; existing teardown warnings remain.

## Warp baseline without MidnaFX

`release120-warp-no-mod` repeats the same Ordon launch and native Hyrule Field
warp (room 0, point 0, layer -1) with an empty isolated mods directory. Ordon
renders before the warp; the destination remains black on repeated captures.
`warp-black.png` records the result. MidnaFX and its AA shader are therefore not
required to reproduce this failure. The cause within host behavior or scene
state is not established, and this destination remains unsuitable for claiming
a successful transition test. The baseline host was closed after capture.

## Successful pond transition

`release120-pond-transition` starts in Ordon with the published package,
AA enabled, neutral exposure, and 20 percent detail. The native Warp panel
selects Lanayru / Fishing Pond, room 0, point 0, layer 0. After the transition
and menu dismissal, the pond, player, water surface, and HUD render normally;
`pond-after-warp.png` records this result. The saved configuration confirms
AA remains enabled. Logs show one active mod, zero thickness-capture failures,
and clean mod unload, with the existing teardown warnings. No validation,
fatal, or temporary input/QA diagnostic markers occur in this run's stdout.

This passes a scene-transition smoke check on D3D12. It does not prove temporal
AA quality or enhanced-water optical correctness (the shutdown summary reports
zero marked draws). The earlier Hyrule Field / layer -1 failure is not a general
failure of native Warp or an AA-dependent failure. Its specific cause remains
unresolved. Both destination and layer differ in the successful test, so this
does not establish that changing the layer alone fixes Hyrule Field.
