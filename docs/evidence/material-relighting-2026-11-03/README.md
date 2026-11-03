# Native material relighting — 2026-11-03

The final Windows/D3D12 candidate uses the same MidnaFX package with both the
preserved stock host and the patched host. The package is a local test build;
the released v1.21.0 binary is unchanged. Exact host, package and patch hashes
are retained in [summary.json](summary.json).

- **Stock host, Ordon:** the requested toggle remains On, the control reports
  unavailable, and original rendering continues. No relighting request reaches
  the host. [Control capture](stock/unavailable.jpg). Exit 0.
- **Initial patched host, Ordon with replacements:** relighting selects the
  native lit-draw shader. Link briefly disappears during asynchronous shader
  warmup, then returns once the new pipeline is ready. The initial
  [settled view](ordon-pack/on-settled.jpg) and log extract are retained. Exit 0.
- **Final patched host, Ordon with replacements:** 1,759 registrations load.
  The encoder records a native fallback draw while the enhanced variant is
  pending, followed by a ready enhanced draw. Off/on/off and re-enable work.
  Live reload restores the saved On request and gameplay continues. Exit 0.
  [Off](ordon-pack-final/off.jpg), [On](ordon-pack-final/on.jpg),
  [Off restored](ordon-pack-final/off-restored.jpg),
  [Reloaded control](ordon-pack-final/reloaded-control.jpg),
  [Reloaded gameplay](ordon-pack-final/reloaded-gameplay.jpg).
- **Final patched host, Fishing Pond with original textures:** startup On
  records native fallback and enhanced draw encoding. The scene, Link, HUD and
  native water render. [Gameplay capture](fishing-original/on.jpg). Exit 0.

The pack's native loader rejects some block-compressed mip dimensions before
relighting activates. Those warnings are separate from the renderer extension;
registration count is not proof that every replacement is loaded or selected.
The only error in the retained native stderr streams is the expected missing
file in each isolated empty memory-card directory.

The first-enable omission was observed live in the initial trial. It is not
present in the final captured views. The final encoder log establishes the
fallback branch and later enhanced pipeline binding, not a frame-by-frame
measurement. Snapshots include unfrozen animation, lighting, HUD and asynchronous
pipeline progress. They do not establish exact pixel parity or full-game art,
motion, alpha or performance qualification.

The final Windows mod and host build successfully. Both source patches pass
forward checks against their pinned Git indices and reverse checks against the
applied files. Eight [focused checks](focused-tests.log) pass. The native draws
exercise generated shaders on D3D12; the separate stock shader check covers the
mod's existing post effects. macOS gameplay remains unavailable and best effort.

See [material relighting](../../material-relighting.md) for setup and scope.
