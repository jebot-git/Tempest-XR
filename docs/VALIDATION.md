# Validation

Validated on Linux, 2026-10-27, with SDL 2.32.10, Mesa 26.0.8 and Intel
ADL-N graphics. Physical headset/WiVRn validation remains outstanding.

The default OpenXR build passes all eight CTest suites. The build configured
with `TEMPEST_OPENXR=OFF` passes its seven applicable suites:

- Gameplay, steering, transitions and ship-follow camera.
- Timed rim combat: source angular windows (including concave-web wrap), early
  and departed-shot misses, capture lanes, pause/jump, rotation/pause tuning,
  continuous animation endpoints across all campaign webs, and impact ordering
  for normal/friendly/laser fire, enemy bullets and spikes.
- Audit regressions, all enemy families and 100 wave generation budgets,
  powerups, both zaps, bonus courses, death/respawn and campaign victory.
- Original tracker decoding, module selection and pause behavior.
- Sample decoding, verified Imagitec positive pitch, original loop regions,
  two/six channel limits, sample-only voices with SFX muted, detuned pairs,
  priority stealing, music context changes/retry, and frozen playback while paused.
- OpenGL sky coverage, original raster/font assets, mesh animation, HUD states,
  bonus scenes, actual rim turns on circular/flat/concave webs, death effects and frame feedback.
- Original module/sample/art hashes and alpha-masked CRY conversion.
- Injected OpenXR runtime failure: play pauses and capture, relative mouse mode
  and XR intent are released before the process exits.

The campaign suite completes **all 24 bonus courses using public wheel/button
controls**, including their forced impulses, speed changes, accuracy awards and
finish gates. It no longer assigns cursor coordinates to pass gates. Generation
budget tests clear spawned enemies to check the 100 schedules; they do not claim
a complete human campaign playthrough.

Bonus clearance ordering, droid expiry/deferred grants, first-ring approach,
family boundaries and ring/psycho completion awards have dedicated regressions.
The bonus camera is covered at entry, exit and return to the destination web
for all three families. Boundary deltas must stay below 0.03 meters per 120 Hz
tick. Explosion/capture paths also test pause, frozen combat, same-wave respawn,
final-life behavior and camera continuity, with a 0.06-meter tick limit.

The campaign and timed rim-combat executables pass AddressSanitizer and UndefinedBehaviorSanitizer.
Regenerating webs, campaign tables, meshes, original art/font maps and sample
metadata from the pinned source reproduces their output bytes exactly.

The isolated native OpenXR fixture (`python3 tools/test_xr.py`) used Monado's
simulated HMD, null compositor and native EGL:

| Scenario | Frames | Stereo submissions |
|---|---:|---:|
| Effects, jump and tunnel flight | 400 | 399 |
| Enemy roster | 90 | 89 |
| Ring bonus | 90 | 89 |
| Tunnel bonus | 90 | 89 |
| Psychedelic bonus | 90 | 89 |
| Explosion/respawn | 90 | 89 |
| Flipper capture | 90 | 89 |

The first session exercised 144 transition frames, reached 13.9995 meters of
the 14-meter flight, and included 102 jumping frames with a late jump across
the web reset. Both death scenes rendered 88 death-animation frames. The first
frame in each session precedes stereo submission. All seven sessions finished
without GL errors or unexpected runtime loss.

Source-art title, bonus and capture screenshots were visually inspected. The
new desktop bonus briefing and stereo headset briefing were also inspected.
An isolated Xvfb progress check also verified the older five-field and current
six-field settings formats and the new nine-field volume format. It restored
wave 1 / `rave4.mod` and wave 17 / `tune7.mod`, retained best score and unlocked
progress, adjusted all three buses with F3/F5/F7, and atomically saved the
expected values. Tests used a private Xvfb display and preferences directory.

These checks establish simulated runtime integration and rendering, rather
than physical tracking, headset comfort or wireless streaming performance.

The DomeD integration suite also passes all 12 checks against an isolated build
of upstream DomeD `0eb4f48`, using a private nested Wayland compositor and a
preloaded test-only OpenXR runtime. It verifies XR/desktop launch, injected
wheel capture, Alt+Tab restoration, reopening XR, V toggling, killed-client
recovery, heartbeat expiry, and rejection by an older compositor. The new
runtime-exception case confirms restoration of desktop/bar presentation and
release of both the native lease and SDL capture before normal game exit.
The running user compositor and system OpenXR runtime were not changed.

These are regression/integration checks, not proof of Jaguar behavioral
equivalence. The [behavior audit](BEHAVIOR_AUDIT.md) catalogs all 41 update
dispatch slots and the remaining AI, collision, tunnel, scoring, frontend and
effect differences. In particular, the 24-course completion test exercises the
prototype tunnel rules, which differ from the original green-track/trail game.
