# Movement camera framing

MidnaFX 1.10 adds **Movement camera framing** to Basic. It widens the exploration
view during brisk movement and settles smoothly at rest. Remaster enables it;
Vanilla disables it. Existing installations retain their saved settings until a
profile is selected. The independent toggle works with **Modern exploration
camera** disabled, using native FOV as its resting baseline.

**Movement FOV boost (%)** in advanced camera tuning defaults to 8%, with a
0–12% range. This is a bounded multiplier in tangent space, not eight extra
degrees. It multiplies the selected static exploration scale when that feature
is enabled. Setting boost to zero bypasses movement framing. Numeric camera
tuning stays unchanged across profile switches; saved presets retain the toggle.

## Behavior and safety

The modifier reads the native player's forward speed and current native maximum
speed without changing either. A smooth response begins above 35% of that maximum
and reaches full framing at 90%. Speed magnitude is used, so either travel
direction is bounded. It does not estimate velocity from position changes, so
teleports cannot create artificial speed spikes. Native speed can still describe
requested motion during collisions; this is not an independent world-velocity
measurement.

The existing camera transition setting controls expansion, defaulting to 0.35
seconds. Return is twice as slow, about 0.70 seconds from a full boost to rest,
to reduce pumping during short input gaps. Zero transition applies immediately.
The envelope advances on native ticks, ignores duplicate callbacks and restarts
after clock rollback, gaps over four ticks, stage/room/player changes, pause,
missing/invalid speed or blocked camera contexts. Static FOV keeps its own
enable transition; movement-only mode cannot pre-warm it.

Only the existing host-approved, active mode-0 chase context can change FOV.
Targeting/aiming, authored events, conversations, cutscenes, detached cameras and
other native algorithms retain native framing immediately. The modifier changes
neither camera position nor target, collision, input, speed, animation or gameplay.
Accepted output follows the host's 10–120 degree bounds. Disabling the feature
removes its contribution immediately.

## Compatibility and evidence

The existing CameraService 1.3 extension is required, just as for modern static
FOV. No additional host patch is added. Unpatched hosts report the feature
unavailable and keep native framing. No render target, shader pass or GPU readback
is added; a wider frustum can still increase the geometry visible to the game.

[The evidence ledger](evidence/movement-camera-2026-10-27/README.md) records
trajectory/clock/reset checks and native runtime results. Whole-game camera feel,
motion comfort, combat/cutscene coverage and broader scene quality remain open.
Users sensitive to changing FOV can turn this independent feature off or set
boost to zero. macOS remains best effort through native builds and portable
checks; native gameplay is unverified.
