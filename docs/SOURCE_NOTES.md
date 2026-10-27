# Source provenance and adaptation

All imports pin [mwenge/tempest2k](https://github.com/mwenge/tempest2k) commit
`8b50b467ce7dd7eee475aaf5ae86804d95401ae7`. Original game: Tempest 2000 (1994),
Jeff Minter / Atari. Source preservation/annotations: upstream contributors.

| Original data/routines | Implementation |
|---|---|
| `web1` … `web37`, `raw_webs`, `sweb` | Original outlines, topology and spawn lanes. Tempest 2000 selects index `16 + ((level-1)&31)` from the shared 48-entry table. |
| `_nw1` … `_nw100`, `setnewgen`, `newgen`, `inits` | All original generation budgets and independent spawn periods. First spawn waits half a period; each subsequent spawn waits a full period. |
| `fz_stuff`, `tz_stuff`, `sz_stuff`, `fuz_stuff`, `pudels`, `pup_stuff` | Original speed, pulse and pickup-delay tables. |
| `run_flipper`, `flipmodes`, `make_sflip2/3`, `make_beast` | Source-angle turns/windows, pause/landing cue, mode-2 rotation, mode-3 down-web flight/away-seeking and directed chains. Super flippers take one hit; beasts shed two reflected mesh parts. |
| `maketank`, `openflippers`, `openfuseballs`, `openpulsars` | Rim/hit opening with paired sample 17, corresponding pairs and source child-entry states; reserved object slots remain adapted. |
| `run_fuseball`, `climbrail`, `run_pulsar`, `run_pspark` | 49-frame rail ride, 16-step crossing/count-based vulnerability, source phase-7 electrification and contact-first timed rim sparks with bounce/wrap. |
| `rumirr`, `refsht`, `run_adroid`, `run_ashot` | Mirror shot takeover/knockback, distinct full-/half-speed reflection rules, shared alien firing counter/cap and hostile-droid four-state patrol/shield/zap. |
| `pupvex`, `hilaser`, `jenab`, `adenoid`, `sprzap`, `mkdroid` | Laser/points/jump/points/droid/token progression and friendly autonomous firing. Existing droids let the pickup sequence skip to the token. |
| `fire`, `r_end1`, `szap_avail` | Full initial Superzapper followed by one-enemy emergency zap. Spikes survive both. |
| `doscore`, `scores` | Centralized scoring and threshold awards, including wave and bonus score. |
| `bonies`, `courses`, `tunnels`, `cg`, `run_gate`, `gatefx` | Three bonus families; original layouts, `(delay+1)*12` emission gaps, up/down impulses, one-eighth boosts, accuracy scoring and finish gates. Token entry and five-level reward keep the late-game skip limit. |
| `modtable`, `webtunes`, `modnum` | Original seven tracker files; title, 16-wave gameplay blocks and bonus selections. |
| `obj2d.s` | Original face topology, intensities and 37 meshes; beast, tanker and jump frames animate. |
| `afont.s`, `bfont.s`, `cfont.s`, `images/*.cry` | All three 96-character font maps and five original sprite sheets, used for the title, score/announcement artwork, bitmap shattering and bonus floor. |
| `ouch`, `zap_player`, `snatch_it_away`, `take_me_to_your_leader` | Polygon breakup, a flipper carrying the claw down-web, camera recoil and same-wave respawn. |
| `fox`, `sayex`, `zapson`, `yeson`, `spup` | Source sample periods/priorities, detuned dual voices, randomized explosion pitch and boost-dependent gate voices. |
| `ringstars`, `initpstarfield` | Animated ring and sine pattern families extended to a spherical XR sky. |
| `ipix`, `any_pixex`, `draw_mpixex`, `draw_pring` | Colored explosion pixels, bitmap breakup, beam rings and trails. |
| `fade`, `ffade` | Per-view scale/rotate/intensity framebuffer feedback (source scale 500/512). |
| `czoom1/2`, `zoomup/down` | Wheel-following XR chase rig, jump recoil and continuous tunnel flight. |

For specific state-machine gaps and corrected behaviors, see the
[behavior audit](BEHAVIOR_AUDIT.md). The table above identifies source inspiration,
not complete behavioral equivalence.

## Import tools

```sh
python3 tools/import_webs.py
python3 tools/import_campaign.py
python3 tools/import_meshes.py
python3 tools/import_art.py
python3 tools/fetch_soundtrack.py
python3 tools/fetch_effects.py
python3 tools/check_assets.py
```

`import_webs` and `import_campaign` accept `--source /path/to/yak.s`;
`import_meshes` accepts `--source /path/to/obj2d.s`; `import_art` accepts
`--source /path/to/tempest2k-checkout`. The asset fetchers accept
`--source /path/to/tempest2k-checkout`. Without those options each reads the pinned
GitHub revision. Generated headers record the original URL/SHA256. Asset
manifests record source URLs, sizes and hashes. No ROM is used.

A web header stores the last vertex index. Closed webs therefore have one more
lane than open webs with the same number of vertices. The repeated vertex and
connection sentinel must not become extra lanes. Original lane orientation words
are also imported, including the internal `w16col` label. Rim rotation uses the
T2K `rospeeds` → `crossdels` → `pauses` selection. `flipcollie` uses a signed
wrapped angle distance ≤52; `colok` uses a strict six-unit depth radius. Coordinates are recentered,
Y-flipped and scaled; the open plane is offset downward for visibility.

The wave header's original annotation says duration, but the executable
`launch1` code decrements it **per generated object** and stops below zero.
Consequently header 24 generates 25 objects. `_nw99` has its own table commented
out and aliases the following `_nw100` data; the importer preserves that behavior.
Spawn ticks use a 60 Hz reference, with a fixed 120 Hz simulation step.

Ring-course words pack delay, signed X, signed Y and gate effect. Their XY
layouts, delays, effect types and eight-course order are imported. After the source-based first-run
approach (about nine seconds for rings), emission gaps use the source countdown and ground speed;
boosts accelerate all approaching gates by one eighth. Up/down gates apply a
vertical momentum change. Psycho mode overrides delay to eight as in `cg`.
`run_gate`/`xbon` accuracy rewards replace flat awards. Tunnel courses use all 64 turn bytes, emitting one turn every 16 segments.
The translated 64-segment buffer, two pattern generators, green-track test,
trail growth/loss and score tiers run at 60 Hz. Although `tundraw` loads
`pongyv` into d6, `inst` clears d6 before `dbra d6,inst`: the preserved active
code advances one segment per frame, rather than the suggested speed ramp.
The mouse changes angular velocity; opposite input brakes. The wall geometry,
RGB colors and control gain adapt the Jaguar mode-7 presentation for XR.

The music files remain original module bytes. `modnum=1` is title, 3/4 are bonus,
and `webtunes` selects gameplay modules in 16-wave blocks (`lsr #4`, despite a
nearby annotation saying 32). libopenmpt is an optional system dependency.

Effects come from the signed 8-bit payloads in `sounds/samples`. The binary
metadata stores lengths as 24:8 fixed point. The fetcher produces lossless
16-bit PCM WAV and preserves each raw payload hash. The converter verifies executable instructions in the included
`dat/moomoo.dat` Imagitec driver. SCLK=27, PLAYFX2 converts pitch via
`floor(pitch*28/43)`, and the DSP adds this as an 8.8 phase increment. At nominal
26.59 MHz, rate is `clock/(64*28)*floor(pitch*28/43)/256`. The old Amiga-period
interpretation was incorrect. Pitch 512 is approximately 19,301 Hz, making the
Recharge sample about 2.25 seconds. Increasing the pitch word increases pitch.

Some preserved payload lengths differ from metadata; the manifest records both
sizes. Playback uses the available payload and bounded source loop regions.
Missing files are silent. There is no synthesized speech or effects fallback.
Source pitch/priority/loop data is generated into `sample_data.hpp`; `sayex` uses
352/354 and `zapson` 200/196, while explosion pitch varies over 128..383.
The actual PLAYFX2 slot loops allow two effects with music, six without music.
Modern stereo interpolation, limiting, separate voice volume and ducking are
adaptations. Exact DSP modulation and the original 50-frame music fade remain
unported. Detailed disassembly addresses and remaining cue differences are in
the behavior audit.

Original polygon vertices, faces and intensities are imported; modern RGB tints,
blending, motion and glow replace the exact Jaguar CRY/polygon/GPU pipeline.
The full original source and executable toolchain are not vendored.

## Audit regression fixes

- Airborne jump time/cooldown survives a web load; the camera follows the same
  continuous trajectory across the coordinate reset.
- Clearance collisions test the whole spike body, including lane entry after
  its tip has passed. Jump protection still permits clearing the obstacle.
- Every score source calls `addScore`, awarding all crossed life thresholds.

`tests/campaign-test.cpp` reproduces all three cases and exercises the roster,
source schedules, powerups, bonuses, late-campaign behavior and victory.

## Raster assets

`import_art.py` converts five original big-endian CRY sheets to alpha-masked BMP,
keeping source and output hashes. `raster_data.hpp` preserves all three original
font coordinate maps plus named sprite regions. Missing UI punctuation has a
small procedural fallback. Unsupported source font rectangles are clipped safely.
The CRY modifier values in `tools/cry_palette.json` are hardware table data from
[Atari's Tom & Jerry reference manual, pp. 30–33](https://asterick.github.io/jag-docs/tom/color-cry.html).
Blue is the reversed-cyan red table, including the manual's corrected 115/17
entries. Intensity multiplication uses the hardware's eight-bit shift.

The death sequence lasts 1.6 seconds with combat frozen. Source polygon faces
scatter for an explosion, or a flipper carries the claw into the web. A smooth
camera path returns to the same wave's starting rim before reloading it. Final
life loss settles into Game Over without another camera discontinuity.

Enemy translations use explicit phase/counter state in `src/enemies.cpp`, with
normal/laser speeds and cadences from `sweb`/`hilaser`/`sclawr`. Spiker selection
and building follow `newspike`/`climbspike`/`descendspike`; spike damage follows
`colok`/`xle3`/`decspike`. In particular, a normal shot spatters harmlessly on a
super spike, while a laser ring is consumed **and returns a shortening hit**.
The reflected mirror ring uses the `ringbull` raster rectangle in `pic2`; shed
beast parts retain the corresponding `hornm2`/`hornm3` meshes. Native actor serials reproduce newest-first source update order, with a live
round-robin bullet table and integer Z-window tests. C++ collections and an
allocation ledger replace physical Jaguar linked-list memory.

## Rotary bonus controls

The primary `m7run` rotary branch accumulates `rot_cum` into `roach`; `iii`
adds twice that momentum to the horizontal position every source frame, with
no horizontal friction. Mouse-wheel notches now add momentum rather than
teleporting X. One nominal notch represents a `$2000` rotary sample; this maps
to .46875 meters/second of horizontal drift at the existing 32-units/meter scale.
Opposite notches brake or reverse it. The source horizontal interval is retained.

Buttons A/C map to held LMB/RMB and use the `iycon` acceleration `$8000`,
friction `$2000` and maximum acceleration/speed field `$80000`, at 60 Hz.
Releasing the buttons coasts with friction. A takes precedence when both are
held. `zup`/`zdn` replace vertical momentum; they do not add an independently
decaying impulse. The initial `iycon` position/velocity and initial viewport
position are distinguished. Source downward Y is converted to upward OpenGL Y
for the gate coordinates and crosshair together.

Physical mouse input now uses `rroco` adaptive sensitivity: `$2000` initially,
`+$1000` per active source update up to `$5000`, and `-$200` on idle updates.
Logical keyboard/test steering remains uncalibrated. The preserved secondary-pad
fall-through runs when a second SDL gamepad is connected, with separate X
inertia and an additional Y inertial pass. `inertcon` checks the previous
position and permits one-step overshoot.

The source's literal `iycon` limit `$1400000`, after `>>3` and meter conversion,
is ±1.25. The preserved course data contains gates beyond that limit, including
Y=2. At the user's explicit choice, normal play uses max(1.25, maximum course
|Y| + .25); `--strict-source-rotary` exposes literal bounds for diagnostics.
Tunnel `tunrun` keeps frictionless rotary momentum and opposite-input braking.

## Gap fills and presentation boundaries

`tests/gap-test.cpp` protects the eight reproduced audit cases, including all
256×256 tunnel phase pairs. `ipix` entry uses a 150-source-update counter,
`setnewgen`/`newgen` integer expiry and actual capacity backpressure. A native
allocation ledger follows `afree=32`, `noclog=21`, tanker reservations and spike
accounting. Player/droid shots, their reflected forms and `xshot` remnants retain
independent eight-slot ownership. Rendering particles do not each consume an
object, just as the source GPU's dots do not each consume a linked-list entry.

`beastrail`, `uppweb`, `flipcollie` and `crossrail` determine hit/zap eligibility.
Mirrors and climbing hostile droids bypass the Superzapper through `mcollie`.
The earlier audit overlooked the `_sz` checks at the start of `run_flipper` and
`run_fuseball`: these make climbing beasts, turning flippers and rail-bound
fuseballs zappable before their normal collision windows. Spark contact occurs
before `collie`; zapped tankers still open their children.
Combat uses a global 60 Hz clock, source actor serial order, bullet-table order,
held-only firing countdowns and the integer `colok` Z window. Camera/interpolation
continues at 120 Hz. New shot takeover keeps actor position and ownership;
`xshot` starts at angle eight, advances three and expires above thirty (eight
updates), while no longer occupying the live bullet table.

`src/fireworks.hpp` contains the pinned `fw_test` bytes (yak.s:19124), executed
by the `fw_run` command interpreter in `src/effects.cpp`. Flight counters,
signed assignments, nested loops, waits, gravity and sphere expiry are translated;
positions/velocities are scaled into XR meters. Ring/sphere and bullet/pickup
feedback have independent lifecycles. `src/cry.hpp` imports the hardware palette bases and first 256 CPU sine bytes.
`render-feedback.cpp` quantizes RGB through a 32³ LUT and preserves CRY hue while
saturating intensity at 384×240 feedback coordinates. Source transform constants
are translated; translucent native composition and meter projection differ from
the GPU/blitter's full integer rasterization. Current HUD/instructions draw
after feedback so they remain legible and do not smear into framebuffer history. Ring/psycho victory
uses the 401-frame `vicrun` interval and a pyramid/echo presentation. Music changes
queue an outgoing fade for `domod`'s 51 frames at 48 kHz (40,800 PCM frames).

The `sweb0` threshold occurs after `czoom2` increments cwave: finishing displayed
web 99 reaches cwave 99 and unlocks Beastly. The 100th schedule is a source alias
retained for diagnostics. Selected-web starts use `slebon`'s arithmetic;
clearance itself adds no points. Point pickups implement `suprise`; One Up uses
the 50-frame holiday except in the tunnel. Source PCM remains the only speech.

The ten default scores and inactive YAK/COW/ZOO/AXE signatures come from
`defaults`. A key is earned beyond displayed web 17; its odd resume web follows
`getinitials`' subtraction/mask. Selected signatures are reused and their source
checkpoint is rewritten at game end. `records.json` version 2 stores the original
16-byte key payload, scores and five-character brag, with version-1 migration.
New games start at one; the native key browser replaces free stage selection.
The 32-character carousel includes DEL and END. Credits/congratulations use
source text and a separate silent game supplies the attract demonstration.
Page layout, mouse/keyboard hints and some transitions remain native adaptations;
Classic/Plus/two-player variants and full original options are outside scope.
