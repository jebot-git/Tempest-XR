# Tempest 2000 behavior comparison

Reference: [mwenge/tempest2k, revision 8b50b467](https://github.com/mwenge/tempest2k/tree/8b50b467ce7dd7eee475aaf5ae86804d95401ae7), reviewed 2026-10-27. Comparison is against Jaguar Tempest 2000, with the other game variants identified separately. Source line references below refer to that pinned revision of `src/yak.s`.

**This is a recreation with imported data, not a complete behavioral port.** Passing the campaign tests establishes that the imported schedules and implemented rules work together; it does not establish that every original enemy state, collision or effect matches. Earlier documentation describing the roster as implemented should be read with the qualifications below.

## Changes from this review

| Issue | Original evidence | Corrected behavior |
|---|---|---|
| Droid survives web changes | `rundroid`, 9191–9205: collected droid deleted when `wave_tim=-2`; only explicit `entities=2` option exempts it | Collected droid expires at clearance and on death. The always-start-with-droid option remains absent. |
| Clearance pickups ignored | `szoom`, 16298; `run_pup`, 10535–10576: clear negative `dnt`, then a clearance pickup sets `dnt=1`; first pickup next web grants early droid without consuming normal sequence | Pickups remain collectible during clearance. One deferred grant survives the web change; an early grant does not become permanent. |
| Bonus interrupts clearance | `czoom2`/`zoo2off`, 13955–13983 increments wave/web; `sweb0`, 8897–8939 then checks bonus | Finish the old-web flight first. Bonus family uses the incremented web index, including the 16/32/48/64 boundaries. |
| First ring arrives too soon | `cg_cnt=$100000`, `cg`, 4890; gate starts at Z=255, accepted at Z≤2 in `run_gate`, 4944 | First-run approach is approximately 8.98 seconds for rings, 7.18 for psycho, replacing the two-second shortcut. Starting Y is `$4000` in source units. |
| Bonus exit adds another old-web warp | `sweb0` returns through `sweb`/`zoom3`, rather than replaying clearance | A separate, pausable 1.5-second XR return joins the destination rim continuously. This duration remains an XR adaptation. |
| Bonus victory score / voice | `m7scr`/`sttoat2`, 4700–4730: `sayex`, `doscore(11)` | Ring/psycho completion awards 20,000 and plays Excellent. Tunnel completion has no invented 20,000 award. |
| Permanent manual music selection | `sweb`, 8632–8641 and `webtunes`, 19152 | N previews a module until the next web/bonus/title context. Automatic selection resumes; failed loads do not falsely replace the current track and retry with a backoff. |
| Voices too slow / easily buried | Actual included Imagitec driver, `dat/moomoo.dat`; details below | Correct positive pitch conversion, source loop regions, two FX channels during music, six without music. No added one-up echo. |
| Wrong clearance voice | `szoom`, `zoomon`, `yeson`, `zoo2off`, `zoom3` | Engine samples during flight, whoosh on ordinary arrival, Recharge after 80/60 seconds. Looping Yes is triggered by a clearance pickup rather than every cleared web. |
| SFX dominates music | Original pause controls independently adjust music and FX (`pausing`, 15015–15160) | Music, effects and recorded voice have independent persistent volumes; defaults 90%, 32%, 90%. Voices duck effects and slightly duck music. Modern mix balance is intentional. |
| Bonus controls unclear | Source uses separate horizontal and vertical controllers (`m7run`) | Desktop and headset instructions explain objective, wheel/horizontal keys, held vertical buttons/keys, impulses, boosts and failure. Bottom bar changes to bonus controls. |
| Synthetic speech | `jenab`, 10702 sets a flag; `adenoid`, 10717 creates the droid; neither invokes speech | Labels remain visual. Only original PCM supplies voices and other sound effects. Missing samples are silent. No TTS dependency, generated speech assets or speech synthesis fallback. |

First-run ring distances account for 60 Hz countdown granularity. The original `cg_cnt` persists between runs and original controls have inertia; this version resets the approach and adapts inertia. Therefore later-run timing is not claimed to be frame-identical.

## Remaining gameplay differences

These are actionable fidelity gaps, not defects proven absent by tests.

| Area | Original behavior | Current behavior / gap |
|---|---|---|
| Flippers | `flipmodes` → `rail`, `flipto`, `stopped`; target-directed flips, rotation windows, pause counters, special rail/stop variants (11007–11559) | Depth/timer lane changes and a persistent random direction. Ordinary flippers can change lanes before the rim. Target pursuit, rotation collision windows and stop-state transitions need a direct port. |
| Super flippers / beasts | `make_sflip2/3`, `superflip`, `railopts`, `sstopmodes`, `beastrail` include different transformations, directed flip sequences and reflected shot forms | HP, speed multipliers and generic reflected bullets approximate these states. Tanker contents do not undergo the source's probability-based super-flipper transformations. |
| Tankers | Three content dispatches, reservations of three object slots; open at top or on hit; paired sound 17 at pitch 60/61 (11620–11817) | Correct content families and pairs, but opening depth is .22 of the web instead of the source rim threshold. Generic kill sound and scoring path; slot reservations and some child states are absent. |
| Spikers / spikes | `spiker_vex` chooses an empty lane or shortest existing spike, build/retreat counters, one-spiker capacity; spikes have independent geometry and optional super state (11844–12105) | Continuous growth, fixed retreat threshold, then adjacent-lane restart; generic 24-second expiry. No shortest-spike selection, source capacity/build counters or super spikes. Shot shortening amounts differ. |
| Fuseballs | `fuse_vex` alternates rail climb and interpolated crossing; collision vulnerable only during particular crossing counts; player-seeking direction (12232–12413) | Player-seeking rail changes exist, but movement, crossing timing, vulnerability windows and geometry are simplified. |
| Pulsars | Source 16-phase global counter, exact phase-7 lane hazard, independent `pulsar_zspeed`, two rim sparks (`run_pulsar`, 12463–12569) | Phase derived from an approximate duty interval; generic speed table path. Mesh/lighting pulse exists, but phase timing and collision do not match exactly. |
| Rim sparks | `run_pspark`, 12575–12607: collision always active at matching depth/lane; bounce at open-web edges | Current sparks are dangerous only during pulse-active intervals and disappear at open edges. This is a known remaining gameplay bug. |
| Mirrors | `rumirr`, 12138–12164: four-count retry state, shot converted in place into ring/reflection, speed-specific knockback | Five-HP approximation, fixed knockback and generic EnemyShot. Reflection identities and exact shot interactions are lost. |
| Hostile droid | `adroidmodes`: `admove`, `adgmove`, `zappage`, `uppweb` (10869) | Generic climber/rim mover with timed lane attack. Full four-state travel, zap and retreat behavior is absent. |
| Friendly droid | `rundroid` uses nearest threat, 16-step movement and a separate shot budget, enlarged by `adenoid` | Nearest-enemy aiming exists; continuous steering and fixed fire delay replace source movement and budget. Lifecycle/deferred pickup is now corrected. |
| Projectile generation | `alienfire`, 10245–10285: global fire counter, bounded `ashots`, eligible enemy states/depths | Per-enemy firing timers from level 9, with a generic child cap. Bullets can accumulate beyond source limits. |
| Player fire | `sweb` initializes shot counters; `sclawr` uses fire delay and `bullmax` slots, with laser/droid changes | Normal/laser cadence, travel speed and slot limits are adapted. No explicit source bullet-slot pool. Swept projectile interception is implemented. |
| Reflected projectiles | `refsht` and `refsht2` have distinct speeds, spin and interception rules | Merged with ordinary EnemyShot, making all reflected forms interceptable with the same rules. |
| Superzapper | `run_objects`/`collie` select enemies on every fourth frame; emergency zap ends after one; source temporarily changes wave speed | First zap destroys all targets immediately; second destroys one. Both visual effects exist, but selection pacing and temporary slowdown differ. Spikes survive. |
| Pickups | `run_pup` has initial rez delay, travel, collection depth window, consumed flash; `pupvex` has eight entries | Six main stages and point repeats exist, but rez/travel/expiry/flash timings are adapted. Rare `suprise` → infinite zap / `outta here` early exit is missing. |
| Bonus token | `sprzap` decrements `warpy`, plays paired Excellent and initiates source-paced zap | Three-token progression, paired sample and enemy clearing exist. Zap pacing, some charge-state handling and exact labels differ. |
| Scoring | Source `scores`/`doscore`, context-dependent `xbon`, transformations and start-level bonus | Fixed per-kind awards, flat 1,000 web-clear reward, 20,000 life thresholds. Not every source score context or starting-level award is ported. |
| One Up / holiday | `do_oneup`, 12717 sets `holiday=50`; `gamefx` cycles web colors and ends the holiday | Life, original sample, bitmap and animation exist; source holiday/color state is not reproduced as a single state machine. |
| Death / respawn | `ouch`, `zap_player`, `take_me_to_your_leader`, `snatch_it_away` and main game loop use source object/wave state and camera phases | A 1.6-second frozen scene followed by full same-wave restart. Original capture geometry is approximated; exact death phases and timers are not preserved. The same-wave restart is implemented independently of the source object/main-loop machinery. |
| Generation / capacity | `_nw` table budgets/periods, `setnewgen`/`newgen`; source object pools, `afree`, `noclog` and spiker restrictions | All 100 imported budgets and periods, initial half delays and final alias match data. 64-entity gate replaces capacity/backpressure rules, so actual encounter timing can differ. |
| Web geometry / camera | Source raw web coordinates, perspective/orientation tables and level-dependent zoom speed | Original outline/topology/start lane and T2K order, recentered/scaled for meters; a tapered 9-meter web, wheel smoothing and fixed 14-meter/2.4-second clearance. |
| Jump | Source accelerated depth trajectory and eligibility in `claw_con1`; disallows initiating during clearance | Fixed .85-second sine trajectory, three-second cooldown, and allows jumping during clearance. This supports the requested XR spike-clearing interaction. |
| Campaign ending | `sweb0` completion/game-end path, source high-score/keys and Beastly behavior | Stops after prototype wave 100 with a victory screen. Source `sweb0` tests `cwave>=99` before starting another wave; this end threshold is not reproduced directly. No Beastly replay, source finish presentation or key awards. |

## Bonus comparison

Ring and psychedelic XY courses, packed delays/types, eight-course sequences, delay override, rectangular hit test, accuracy scoring and one-eighth boosts are imported or translated. Ring hits use 250/500/750; psycho preserves the unusual source lookup order. Vertical impulses, controls, speed integration, failure/victory scene duration and feedback are adapted. The source ring victory presentation lasts much longer and uses a pyramid/feedback scene; the current result holds for two seconds before the XR return.

**Tunnel is still a simplified stand-in.** Original `_tunn`/`tunrun`/`tundraw` (3920–4490) uses an inertial rotating tunnel with a green track, 64-segment circular buffer, pattern generators and a growing/shrinking player trail (`ltail`). Staying on the track increases the trail and earns continuous score; leaving it consumes trail before failure. Course turns arrive through the tunnel segment generator. Current code reuses the 64 turn-byte sequences as radial openings, requires every opening to be passed, awards 250 per opening, and fails immediately on a miss. Source trail tolerance, continuous scoring, speed progression and tunnel wall patterns are absent. Its two-second opening approach is also an adaptation. Completing the prototype tunnel courses does not validate the original tunnel rules.

## Sound and music evidence

The preserved metadata calls its pitch field a period, but the executable treats it as a positive phase increment. `yak.s` includes [`dat/moomoo.dat`](https://github.com/mwenge/tempest2k/blob/8b50b467ce7dd7eee475aaf5ae86804d95401ae7/src/dat/moomoo.dat), mapped at `$4040`. The importer verifies its decoded SHA256:

`37c466acc81e94c66273dd5c3584ece04d46e2912f53ba8da7db946e3d6d87bc`

- `$4146`: set `SCLK=27`, `SMODE=21`.
- `$4440`: `mulu #28,d3; divu #43,d3; and.l #$ffff,d3` in PLAYFX2.
- DSP program at `$5ed4`, offset `$8a`: add pitch to phase; offset `$ac`: shift phase right eight bits for sample indexing.
- `$4394` / `$43a2`: effect slot loop count 1 or 5, meaning 2 or 6 slots. The source's prose comment claiming four FX alongside music is inconsistent with these instructions.

At nominal NTSC clock 26.59 MHz, `rate = clock/(64*28) * floor(pitch*28/43)/256`. Pitch 512 gives about 19,301 Hz; Recharge lasts approximately 2.25 seconds. The old Amiga-period conversion produced 6,928 Hz and made it take over six seconds. Runtime overrides use the ratio of converted positive steps. WAV payloads are unchanged; only their header sample rates changed. Loop start/end metadata is preserved and bounded to the available payload.

This is still a modern interpolating stereo mixer, not DSP emulation. It uses modern limiting, volume buses and ducking. It does not reproduce exact hardware volume quantization, every CHANGEFX modulation, sample truncation, phase-start behavior or priority tie selection. Engine pitch follows the XR transition, not every original zoom iteration. Some contextual cues still use a generic explosion instead of source-specific tanker/spike sounds. The archived `unused/syn6.o` DSP payload is byte-identical to the included driver's DSP program, but the conversion checks use the included data itself.

Original music selection is: title `tune13`, waves 1–16 `rave4`, 17–32 `tune7`, 33–48 `tune5`, 49–64 `tune12`, then repeat; ring/psycho `tune1`, tunnel `tune3`. It does **not** select a different tune every web. Selection now works across these boundaries and bonus returns. libopenmpt decodes original module bytes; the original `domod` 50-frame fade-down/wait is replaced by immediate module replacement with a short fade-in. The original explicit tune-lock option is absent; N is a temporary preview.

## Complete object-dispatch inventory

Every slot in the original update dispatch is classified below. “Adapted” indicates a corresponding behavior or visual, not an instruction-for-instruction port. This inventory plus the subsystem tables identifies coverage; it is not a claim of exhaustive branch equivalence.

| `run_vex` index | Source handler | Current coverage |
|---:|---|---|
| 0 | `rrts` | No-op represented by mode/state checks |
| 1 | `player_shot` | Adapted Shot travel/lifetime |
| 2 | `run_flipper` | Adapted; state-machine gaps above |
| 3 | `run_zap` | Adapted explosion/flash |
| 4 | `kill_shot` | Shot removal |
| 5 | `run_tanker` | Three content families, adapted opening |
| 6 | `run_spike` | Lane spike array, missing super state |
| 7 | `run_spiker` | Adapted climb/retreat, missing selection states |
| 8 | `run_ashot` | EnemyShot, adapted fire/collision budgets |
| 9 | `run_fuseball` | Adapted pursuit/crossing |
| 10 | `blowaway` | Generic particle lifecycle |
| 11 | `run_pulsar` | Adapted pulse/climb |
| 12 | `oblow` | Generic particles / One Up |
| 13 | `go_downc` | Capture camera adaptation; no two-player claw lifecycle |
| 14 | `go_downf` | Capture flipper adaptation |
| 15 | `claw_con2` | Missing second player |
| 16 | `claw_con1` | Wheel/buttons/keyboard adaptation |
| 17 | `rez_claw` | Invulnerability / respawn adaptation |
| 18 | `czoom1` | XR clearance flight adaptation |
| 19 | `czoom2` | XR next-web join / bonus dispatch |
| 20 | `pzap` | Single-player death visual; separate multiplayer path absent |
| 21 | `rundroid` | Adapted autonomous droid; lifetime corrected |
| 22 | `run_pixex` | Bitmap/glyph particles, adapted physics |
| 23 | `run_pspark` | Rim sparks; hazard/edge gaps above |
| 24 | `run_prex` | Generic polygon/sphere particles |
| 25 | `run_pup` | Pickups, early droid; animation/rare branches absent |
| 26 | `xshot` | Generic projectile impact; no distinct source ring-shot lifecycle |
| 27 | `run_gate` | Ring/psycho gate translation, adapted presentation |
| 28 | `xr_pixex` | Bitmap burst/travel adaptation |
| 29 | `run_fw` | Generic victory particles; source firework lifecycle absent |
| 30 | `run_h2hclaw` | Missing head-to-head |
| 31 | `run_mirr` | Auxiliary spinning mirror draw represented generically |
| 32 | `run_h2hshot` | Missing head-to-head |
| 33 | `run_h2hgen` | Missing head-to-head |
| 34 | `run_h2hball` | Missing head-to-head |
| 35 | `oblow2` | Generic explosion particles |
| 36 | `rumirr` | Adapted campaign mirror |
| 37 | `refsht` | Generic EnemyShot; distinct reflection behavior absent |
| 38 | `refsht2` | Generic EnemyShot; distinct half-speed/spin behavior absent |
| 39 | `run_adroid` | Adapted; four-state AI absent |
| 40 | `loiter` | Respawn countdown; separate source multiplayer reserve-life behavior absent |

All twelve `inits` entries map to a Kind: flipper, tanker, spiker, fuseball, pulsar, fuse tanker, pulsar tanker, super flipper, mirror, hostile droid, beast, second super flipper. The secondary dispatches (`flipmodes`, `railopts`, `sstopmodes`, `spiker_vex`, `fuse_vex`, `adroidmodes`, `pupvex`, `gatefx`, `bonies`) reveal the unported states described above. Importing all twelve generator types does not fill those states automatically.

## Drawing, special effects and frontend

All 18 `draw_vex` entries were classified: `rrts`, `draw`, `draw_z`, `draw_vxc`, `draw_spike`, `draw_pixex`, `draw_mpixex`, `draw_oneup`, `draw_pel`, `changex`, `draw_pring`, `draw_prex`, `dxshot`, `drawsphere`, `draw_fw`, `dmpix`, `dsclaw`, `dsclaw2`. Modern meshes, lines, bitmap particles, beam rings and death scenes cover their visible families. Distinct source transform modes, lifecycle counters, firework sequences and multiplayer claw variants are not fully ported.

The 27 `solids` slots include no-op, flipper, three tankers, shot, fuseball, pulsar, ring bullets, pixel explosions, pickups, gates, H2H claw/shot/generator/ball, mirror, ring-shot impact, blue flipper, two super flippers, three beast stages and hostile droid. Original imported meshes represent the campaign forms. H2H logic is absent despite some associated shapes being present in the data.

| System | Present | Remaining difference / omission |
|---|---|---|
| Polygon drawing (`obj2d.s`, `horse.gas`) | Original face topology and vertex intensity, animated mesh frames | Modern RGB lighting/blending; source projected orientation, CRY shading, clipping and animation counters differ |
| Raster/font drawing (`afont`, `bfont`, `cfont`, `images`) | Five imported sheets, all three glyph maps; title, messages, scores, One Up, Game Over | Modern layout and glyph fallback; source scaling, timing, palettes and every frontend graphic use are not reproduced |
| Starfield (`llama.gas`, `goat.gas`; `ringstars`, `siney`) | Full-sphere XR stars, ring/sine patterns, streaks | Spherical distribution and modern animation replace the source projected field buffers |
| Mode 7 (`ox.gas:mode7`) | Textured ring/psycho floor using original art | No source GPU raster projection, scanline arithmetic or exact ground animation |
| Tunnel (`ox.gas:starseg`, `voo`; `run_pgens`) | Radial openings / connecting geometry | Missing original wall pattern generators, particle-object trail and green-track gameplay |
| Melt-o-vision / feedback (`fade`, `ffade`, `gofeed`) | Per-eye and desktop scale/rotate/intensity framebuffer histories | Different triggers, rotations, colors, decay and result-screen durations; not pixel-identical GPU feedback |
| Explosions / messages | Original bitmap breakup, colored pixels, mesh fragments, score bursts | Modern random spread/lifetimes and generic debris replace individual source handlers |
| Zapper / jump / warp | Arcs, flashes, trails, rainbow beam rings, continuous next-web tunnel | XR distances/durations/camera constraints are intentional additions; source flicker/slowdown and exact color timing remain different |
| Attract mode / tutorials / credits | Modern title and optional autoplay demo | Missing original cycling demos, enemy demonstrations, tutorial pages, credits and full title sequence |
| Original options | Modern wheel/audio/autofire settings and unlocked web selection | Missing entity/player selection, permanent droid-start option, original controller mapping, tune lock and full option screens |
| Classic / Plus / cooperative / H2H | No playable implementation | Separate source game variants are out of current scope |
| High scores / EEPROM (`eeprim.s`) | Best score, unlocked web and preferences saved to SDL path | No initials/table, EEPROM-compatible records, keys/password/tag algorithms, original start-bonus restoration |
| Display/input timing | Fixed 120 Hz simulation with render-independent time | No PAL/NTSC interrupt timing, original rotary acceleration or original object-pool ordering |
| XR / desktop integration | Native OpenXR, tracked eye poses, ship-follow rig, wheel capture, pause/focus recovery, desktop/SBS modes | Added functionality. Controller bindings and physical headset/WiVRn comfort/performance testing remain outstanding |

## Validation limits and next priorities

Regression tests exercise the corrected droid lifecycle/deferred grant, clearance-before-bonus order, bonus family boundaries, first-ring distance, ring/psycho completion award, continuous and pausable bonus return, automatic music selection after preview, failed-track retry, source PCM rate/pitch/loops, two/six channel limits and sample-only voices. All 24 prototype bonus courses are completed with public controls. This does not validate the absent tunnel rules or exact Jaguar AI.

Priority order for further source fidelity: (1) flipper/super/beast and hostile-droid state machines, (2) pulsar spark collision/edge behavior and exact pulse phases, (3) original tunnel track/trail/scoring, (4) source firing/capacity rules and spiker selection, (5) score contexts/rare pickups/one-up state, (6) remaining individual effects, music fade and frontend variants. Replacing these with faithful state transitions requires dedicated regressions; it cannot be established by existing roster and rendering tests alone.
