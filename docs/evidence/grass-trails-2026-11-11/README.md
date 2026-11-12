# Recovering movement trails — November 11, 2026

The existing Ground cover control now includes bounded movement histories for
Link and Epona. Near grass and low plants keep a directional bend briefly after
contact, then recover. Original terrain, collision, stones, litter, moss and
far grass proxies retain their existing behavior.

## Focused proof

`terrain_trails`, `grass_view` and `package_contract` all pass after the final
build. Runtime motion math is tested directly for segment creation, finite
bounded effects, recovery, standing still, teleport rejection, vertical and
horizontal exclusions, bounded storage, invalid clock reset, explicit clear and
the wider second actor corridor.

## Native Windows D3D12 probe

Hyrule Field `F_SP121,6,2,4`, original textures, default mod settings. The isolated
preview's Port 1 device is assigned Keyboard through its native UI. Native key
binding capture does not accept short automated key taps, so this does not
establish ordinary walking or riding visual quality.

The native console reports Link at approximately `-36608.93 -6963.36 97040.24`.
A controlled short position change uses `tp -36518.9 -6963.35 97040.24`.
The runtime logs:

`Ground movement trails active: 276 nearby clumps, 12 retained segments; gradual recovery`

The [probe capture](probe.png) is an unfrozen runtime context image. Capture
latency exceeds a substantial portion of the recovery time, so it is not an
isolated fresh-trail versus recovered comparison. No claim is made that all
logged segments come from the single controlled change. The native pass compiles
and renders without logged WGSL/GPU validation errors. Normal close returns exit
code 0. The clean trial also logs the existing missing `gczelda2` card file.

Native-probe package SHA256:
`4E40D28F7069A5066D448B63E0717DEBDF246001678397A9E379AE1547050F67`.
Final package SHA256:
`59128C7F18423F805FF575EEF284858F1843E6B541D80D5A9AABDA08922AEB96`.
Only source indentation changes between these builds. Final focused checks pass.
Host SHA256:
`FDBDD3F535351732538BC379A7E75BCCD6707D2F3C8BEF744C680EC3DD11BEA3`.

## Limits

Continuous walking/riding appearance, native foot/hoof contact fidelity, camera
motion stability, GPU/CPU duration and macOS gameplay remain unverified. The
history is an actor-position approximation, capped at sixteen segments per actor.
Fast travel can replace old segments before their nominal recovery lifetime.
