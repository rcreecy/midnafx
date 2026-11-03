# Native material relighting

Material relighting evaluates each supported native material's RGB lighting at
the fragment instead of interpolating lighting calculated at vertices. It uses
the material's real normal, ambient and material color sources, active light mask,
and native diffuse, spot and specular attenuation. The original texture and TEV
combiner stages still determine the final surface color. No asset list or texture
filename classification is involved.

Enable **Basic / Material relighting (experimental)** or select **Remaster** on a
compatible host. Vanilla disables it. The independent setting defaults off.
Existing saved scene looks without this feature bit load with relighting off.
Stock hosts keep native rendering and report that the control is unavailable.

## Host extension

The mod imports optional `dev.twilitrealm.dusklight.native-material` 1.0. The
stock SDK can build the mod; stock Dusklight does not expose the service. Build
the pinned host and its Aurora submodule after applying the two source patches:

```powershell
python tools/material-relighting-host-patch.py --dusklight-dir path/to/dusklight
python tools/material-relighting-host-patch.py --dusklight-dir path/to/dusklight --apply
```

The first command checks applicability without editing files. `--reverse` removes
both patches from the same pinned source checkout. Build Dusklight using its
native build procedure and keep the executable name `dusklight.exe`. Use a
separate host directory and install the MidnaFX mod package normally.

The service tracks one game-thread request owner. Scene boundaries record one
copied mode byte in FIFO order; the render worker receives no mod pointers or
callbacks. The host ends the scope before HUD rendering. Disable and detach
release ownership. Scope changes invalidate the cached pipeline; the mode is
part of the shader key. Projection or offscreen eligibility changes also prevent
reuse of an incompatible pipeline. This path adds no stage callback drains,
offscreen targets, draws, or uniform fields. Existing uniforms are rebuilt when
the native pipeline cache invalidates them.

Both variants share the same uniforms and bindings. While an enhanced pipeline
compiles asynchronously, the encoder draws with the ready native pipeline. This
avoids holes during first enable without waiting for shader compilation. The
host draw record reuses its redundant vertex range slot for the fallback
reference and retains the existing inline payload size.

## Coverage and preservation

Supported draws are depth-tested, color-writing, main-target perspective
triangles with a native normal attribute and a TEV-sampled lit RGB channel.
Native strips/fans/quads use their ordinary triangle conversion. Register and
vertex color sources, both native RGB channels, transformed/skinned normals,
native light masks and animated texture/color state use the same shared path.
Offscreen buffers, orthographic draws, lines, points, unlit materials and draws
without normals keep the native shader.

The shader normalizes the interpolated view-space normal. An unusable normal or
position retains the interpolated native RGB result. Invalid light distance,
coefficients or attenuation are guarded in the enhanced path. It preserves the
original vertex alpha path, including when a TEV raster swap uses native RGB as
an alpha input. The existing alpha test, blend order, fog, depth, texture
bindings, replacement selection and matrices remain in the native pipeline.

This covers the native light model. Baked lighting and unlit surfaces do not
provide a recoverable dynamic lighting model. Physical roughness, metallic and
normal-map inputs need authored data and a compatible material shader contract.
No PBR maps, extra light transport or new shadows are inferred. The catalog's
original asset identities remain available for future authored profiles.

The enhanced shader keeps the vertex RGB fallback and adds fragment work for
lit channels. No GPU duration or 4K performance claim is made. macOS support is
best effort; only the working Windows host can exercise native rendering here.
Hosted Metal execution is excluded.

The [Windows evidence](evidence/material-relighting-2026-11-03/README.md) records
stock-host fallback, original and replacement texture trials, native warmup
fallback, enhanced draw encoding, toggle, live reload and normal close.
