# Surface-aware contact lighting

MidnaFX 1.16 improves the surface reconstruction shared by **Ambient occlusion**,
**Contact shadows** and **Screen-space color bounce**. Select Remaster or enable
those existing controls independently. Their strengths, saved settings and
disable behavior remain unchanged. No asset identities or stage exceptions are
added.

The earlier reconstruction chose each normal derivative from the smaller
view-depth difference at its two neighboring pixels. Beside a steep surface, an
unrelated surface can have a smaller difference and reverse the reconstructed
normal. This can create false AO and misdirect the normal-dependent shadow,
bounce and filtering calculations.

The new reconstruction compares two samples on each side. Device depth is affine
over a projected plane; extrapolating each pair back to the center identifies
which side better continues that plane. The comparison works with forward and
reversed depth. Differences below 0.0000002 retain the nearest-depth choice to
avoid following numerical noise. If either second sample is unavailable or
invalid, reconstruction falls back to the nearest valid first neighbor.
Background, non-finite depth and neighbors outside the viewport cannot supply a
derivative. An unavailable axis produces an invalid lighting field rather than
an invented normal. Existing derivative-length limits remain.

The change operates only at the existing half-resolution representatives. It
adds up to four depth reads and four view reconstructions per valid field pixel,
with no new target, compute dispatch, fullscreen draw, history or CPU readback.
GPU duration and universal performance are not established. Existing surface-aware
filters and full-resolution reconstruction consume the improved field normals.
Scene alpha, native HUD ordering and independent fog/sunlight behavior remain.

This reconstructs visible opaque depth geometry, not authored or smoothed vertex
normals. It cannot create normal-map detail, recover hidden surfaces, eliminate
thin-geometry limits or guarantee a correct derivative at every intersection.
Broader native art, motion and performance evaluation remains open. See the
[evidence ledger](evidence/contact-lighting-2026-10-29/README.md) for baseline
failures, analytic geometry proof and native lifecycle observations.

macOS support remains best effort through native package builds and portable
checks. Native Mac gameplay and Metal execution are unverified; hosted Metal
execution is excluded under the working-host policy.
