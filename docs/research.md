# Research baseline and M1 decision

Inspected 2026-09-16: [Dusklight](https://github.com/TwilitRealm/dusklight)
`edf42c6a7202647b56dd2fcdef02d17671bc814b`, its Aurora submodule
`7f2801cd0133c9333eadb4e2e6b24100c328d328`, official
[mod template](https://github.com/TwilitRealm/mod-template)
`ece6d0dae843675fbd1e3f2308a0f1f782da5d80`, and the public
[Dawnlight fork](https://github.com/BeZide93/dusk)
`74b0351c1aa647a4e1945c62a1192006fa50f713`. Local source roots are
`upstream/dusklight`, `upstream/mod-template`, and `upstream/dawnlight`.
Source inspection and a Windows compile/package are the evidence level; no game runtime was
available. Paths below are relative to Dusklight unless prefixed. Detailed symbol traces are
in [render](notes/render.md), [game](notes/game.md),
[UI/config](notes/ui-config.md), [shader/performance](notes/shader-performance.md), and
[integration review](notes/review.md).

| Question | Source evidence and observed behavior | M1 implication |
|---|---|---|
| Mod format/load/reload | `docs/modding.md` Native Mods/Runtime Lifecycle; `src/dusk/mods/loader/loader.cpp::deactivate_mod`, `reload_runtime_mod`; `cmake/ModSDK.cmake::add_mod` | Native C++20 module in a `.dusk` ZIP, with `mod.json` and `lib/<platform>-<arch>/mod.dll` or `mod.so`. Packaged mods can reload; in-place native libraries cannot. |
| Build/SDK/platforms | `sdk/CMakeLists.txt`, `cmake/{GameABIConfig,ModSDK}.cmake`; template `CMakeLists.txt`, CI workflow | `FEATURES webgpu` gives WebGPU C headers; no game ABI needed for M1. SDK downloads pinned Dawn package and game link stub. Template CI lists Windows/Linux/macOS x64+Arm, iOS/Android targets; current M1 is compiled for Windows x64 only. |
| Game state | `sdk/include/mods/svc/game.h`, `stage.h`; `src/d/d_kankyo.cpp::dKy_darkworld_check` | GameService is ABI epoch, not an environment getter. Future M5 may use game symbols; active Twilight is state exactly 1, not generic nonzero. |
| UI/settings | `sdk/include/mods/svc/{ui,config}.h`, `src/dusk/mods/svc/{ui,config}.cpp` | Host UI is versioned UiService 2.3 using RmlUi documents, not an ImGui mod API. ConfigService 1.0 persists mod vars in host config.json. M1 exposes enable and diagnostics only. |
| Graphics API/backend | `sdk/include/mods/svc/gfx.h::GfxService`; `extern/aurora/lib/webgpu/gpu.cpp` and Aurora backend selection | WGSL shaders through WebGPU C; Dawn uses Metal on supported macOS backend. GfxService does not directly report the selected backend. Runtime detection and Intel GPU behavior remain open. |
| Frame stage | `src/m_Do/m_Do_graphic.cpp::mDoGph_Painter` around 2759; `src/dusk/mods/svc/gfx.cpp::gfx_run_stage` | `GFX_STAGE_FRAME_BEFORE_HUD` occurs after scene and native bloom, before ordinary HUD lists. Some wipes/2D effects precede it; same-stage third-party ordering is unspecified. |
| Scene snapshot/fullscreen | `gfx.h::{resolve_pass,push_draw,register_draw_type,get_scene_target_layout}`; `extern/aurora/lib/gfx/{recording,encoding}.cpp::resolve_pass`, `CopyTextureToTexture`; `mods/ao_mod/src/mod.cpp::on_draw` | Snapshot is single-sample scene color and entails a full-size copy/pass break. Continue drawing into EFB with one fullscreen triangle; no mod-owned full-size target. Zero-copy feedback is unsupported. |
| Formats/gamma/resolution | `gfx.h::GfxRenderTargetLayout`; `extern/aurora/lib/gfx/{frame,recording}.cpp`; `lib/webgpu/gpu.cpp` | Query dimensions/layout each stage; accept only single-sample RGBA8/BGRA8 UNORM for M1. Source suggests SDR UNORM, but actual transfer interpretation and exact neutral parity require capture. MSAA cannot preserve per-sample content through this pass. |
| Lifetime/sync | `gfx.h` ownership contract; `src/dusk/mods/svc/gfx.cpp::gfx_mod_deactivating`; `extern/aurora/lib/gfx/render_worker.cpp::synchronize` | Snapshot views valid for current frame only. Host drains worker encodes before shutdown. No per-frame GPU wait. Retained pipeline/layout released at shutdown; bind group is frame-local. GPU completion is not promised by worker drain. |
| Pipeline/shader | `mods/ao_mod/src/mod.cpp::build_composite_pipeline`, Aurora `gfx_init_color_target_states` | Create once at init and keep until shutdown; UI toggles do not rebuild. Unsupported layout changes bypass. No runtime WGSL recompilation. |
| Config/files/logging | `sdk/include/mods/svc/{config,host,file,resource,log}.h`; `src/dusk/mods/svc/config.cpp::config_flush_if_dirty` | Host config is saved on a throttled interval. `HostService::data_dir` is persistent for future custom presets; `mod_dir` is scratch. Log state changes, not frames. |
| Profiling | `gfx.h::GfxService` and Aurora GPU profiler source | No public GPU timer service. CPU stage recording can be measured and is distinct from GPU cost. GPU pass/copy times require target runtime capture. |
| Dawnlight | `upstream/dawnlight/mods/dawnlight/src/mod.cpp`; `notes/game.md` | Current public package installs gameplay/UI hooks. Source search did not find its own grade/bloom pass; co-loading and installed versions remain unverified. |

Original Twilight appearance arises from multiple native inputs. `src/d/d_kankyo.cpp`
interpolates environment palette, fog, lights and bloom/mono parameters;
`src/d/d_kankyo_data.cpp` has separate Twilight, Twilight Weak, Senses and Palace bloom
records; weather/actor code adds particles, mist, sky, and portal effects. `m_Do_graphic.cpp`
composes these through native bloom and framebuffer operations. Source cannot apportion the
undesirable appearance between original game, Dusklight, Dawnlight and texture replacements
without identical-scene captures. Parameter changes could improve fog/bloom locally but
cannot replace general grading. Architecture choice is **C over the long term**: a generic
post-process layer plus separately switchable verified environment-parameter enhancements.
For M1, only the generic passthrough portion is implemented. See [architecture](architecture.md).

Open gates: Intel Mac/Metal startup, shader validation and neutral captures; exact HUD/2D
inclusion under host settings; Dawnlight co-load; resize and unload; GPU copy/draw timings;
texture pack interaction; game-state transition semantics; and safe profile tuning. No
performance or visual improvement claim follows from the current source review.
