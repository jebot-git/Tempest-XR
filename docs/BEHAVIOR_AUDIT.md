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
| Flippers | `flipmodes` → `rail`, `flipto`, `stopped`; target-directed flips, rotation windows, pause counters, special rail/stop variants (11007–11559) | Straight climb, source orientation/rotation/pause data, wrapped 52-unit shot window, vacated-lane capture and continuous pivot animation. Impacts resolve independently of storage order. Tanker entry and directed chains are translated; artificial expiry is removed. |
| Super flippers / beasts | `make_sflip2/3`, `superflip`, `railopts`, `sstopmodes`, `beastrail` include different transformations, directed flip sequences and reflected shot forms | Super flippers take one valid hit. Mode 2 rotates twice as fast; mode 3 turns away while climbing below the rim, then pursues. Tanker mutations use source probabilities and directed chains. Beasts shed two reflected parts while climbing, then die for 150; the renderer composes the remaining source meshes. Source climb speed replaces invented multipliers. |
| Tankers | Three content dispatches, reservations of three object slots; open at top or on hit; paired sound 17 at pitch 60/61 (11620–11817) | All three families open on a valid hit or at the rim, award 100 and play paired sample 17 at pitches 60/61. Flipper/pulsar children enter pivoted turns; fuseball children start opposite rail crossings. Three-slot reservations remain absent. |
| Spikers / spikes | `spiker_vex` chooses an empty lane or shortest existing spike, build/retreat counters, one-spiker capacity; spikes have independent geometry and optional super state (11844–12105) | Empty-lane/shortest-spike selection, initial 30-unit spikes, 51 extension updates before retreat, maximum height and one-spiker limit are translated. Normal shots shorten ordinary spikes by three units, with source score/deletion awards. Super spikes reject normal shortening; laser rings shorten them but are consumed. Spike object-slot accounting remains adapted. |
| Fuseballs | `fuse_vex` alternates rail climb and interpolated crossing; collision vulnerable only during particular crossing counts; player-seeking direction (12232–12413) | 49-frame rail ride, 16 two-frame crossing steps, source seeking direction, asymmetric collision-lane changes and crossing-count 4–12 vulnerability. Rail position/crossing geometry share the same state. Laser also requires this vulnerability; zap remains effective. |
| Pulsars | Source 16-phase global counter, exact phase-7 lane hazard, independent `pulsar_zspeed`, two rim sparks (`run_pulsar`, 12463–12569) | Source 16-phase countdown (+1 frame on signed expiry), phase-7 hazard, source mesh cycle and independent .5-unit/frame climb speed. Two rim sparks begin in the parent lane. |
| Rim sparks | `run_pspark`, 12575–12607: collision always active at matching depth/lane; bounce at open-web edges | Contact precedes shots regardless of pulse phase; five-frame propagation, open-end bounce and closed-web wrap. Valid shot kills award 150. |
| Mirrors | `rumirr`, 12138–12164: four-count retry state, shot converted in place into ring/reflection, speed-specific knockback | Five valid hits unless knocked beyond the bottom sooner; fuse speed, 16-speed-unit knockback and source ring-raster reflections. Surviving hits convert normal, friendly and laser shots. A fatal hit exits before conversion. Mirrors do not use the Superzapper collision path. |
| Hostile droid | `adroidmodes`: `admove`, `adgmove`, `zappage`, `uppweb` (10869) | Climb/starting/crossing/zapping states translated: double-speed approach above the rim, shielded 32-speed-unit knockback, 16-step patrol, halfway collision-lane change and 32-frame zap. Jumping shots originate at ship depth and can reach the overhead patrol. Climbing shields also ignore zap. |
| Friendly droid | `rundroid` uses nearest threat, 16-step movement and a separate shot budget, enlarged by `adenoid` | Nearest-enemy aiming, corrected expiry/deferred pickup and shared enlarged shot limit. Continuous steering and its fixed fire delay still replace source 16-step movement and separate firing entitlement. |
| Projectile generation | `alienfire`, 10245–10285: global fire counter, bounded `ashots`, eligible enemy states/depths | Shared 113-call alien-fire countdown, eligible flipper/spiker states, minimum firing depth and per-wave 1–4 alien-shot cap. Ordinary alien shots use source speed; tankers/pulsars do not gain invented gunfire. Capacity/backpressure still uses the modern entity collection. |
| Player fire | `sweb` initializes shot counters; `sclawr` uses fire delay and `bullmax` slots, with laser/droid changes | Source normal/laser speeds, four-/three-frame cadence and 8/16 total active-shot ceiling, retaining slots occupied by reflections. Swept strict six-source-unit impacts resolve in time/depth order. Explicit slot ownership, original object-list ordering and friendly-droid firing entitlement remain adapted. |
| Reflected projectiles | `refsht` and `refsht2` have distinct speeds, spin and interception rules | Mirror rings ignore player shots and move at current shot speed; beast fragments move at half speed, survive/absorb normal shots and allow laser through. Both retain shot capacity, ignore zap, can travel beyond the rim and do not block wave clearance. |
| Superzapper | `run_objects`/`collie` select enemies on every fourth frame; emergency zap ends after one; source temporarily changes wave speed | First zap destroys eligible targets immediately; second destroys one. Mirrors, reflections and climbing hostile-droid shields ignore it, as their handlers bypass collie. Spikes survive. Source every-fourth-frame selection pacing and temporary slowdown remain absent. |
| Pickups | `run_pup` has initial rez delay, travel, collection depth window, consumed flash; `pupvex` has eight entries | Six main stages and point repeats exist, but rez/travel/expiry/flash timings are adapted. Rare `suprise` → infinite zap / `outta here` early exit is missing. |
| Bonus token | `sprzap` decrements `warpy`, plays paired Excellent and initiates source-paced zap | Three-token progression, paired sample and enemy clearing exist. Zap pacing, some charge-state handling and exact labels differ. |
| Scoring | Source `scores`/`doscore`, context-dependent `xbon`, transformations and start-level bonus | Flipper/spiker/beast/spark 150, tankers 100, pulsars 200, spike hit/deletion awards and xbon random 250/500/750/1 on mirror/droid/fuseball destruction. Flat 1,000 web-clear reward, point pickups and selected-start-level awards still differ; 20,000-point extra-life thresholds exist. |
| One Up / holiday | `do_oneup`, 12717 sets `holiday=50`; `gamefx` cycles web colors and ends the holiday | Life, original sample, bitmap and animation exist; source holiday/color state is not reproduced as a single state machine. |
| Death / respawn | `ouch`, `zap_player`, `take_me_to_your_leader`, `snatch_it_away` and main game loop use source object/wave state and camera phases | A 1.6-second frozen scene followed by full same-wave restart. Original capture geometry is approximated; exact death phases and timers are not preserved. The same-wave restart is implemented independently of the source object/main-loop machinery. |
| Wave-clear condition | `r_end` / `szoom`, 16290–16306: after generation ends, clearance begins when the furthest enemy is at the rim | After generation finishes, clearance starts when all counted enemies/alien bullets reach the rim. Pickups, spikes and reflections do not block it; live rim enemies survive into flight. |
| Generation / capacity | `_nw` table budgets/periods, `setnewgen`/`newgen`; source object pools, `afree`, `noclog` and spiker restrictions | All 100 budgets/periods, initial half delays and final alias match imported data; one-spiker restriction is enforced and failed spiker launches consume generation budget. The 64-entity gate still replaces source afree/noclog and reserved tanker/object slots. |
| Web geometry / camera | Source raw web coordinates, perspective/orientation tables and level-dependent zoom speed | Original outline/topology/start lane and T2K order, recentered/scaled for meters; a tapered 9-meter web, wheel smoothing and fixed 14-meter/2.4-second clearance. |
| Jump | Source accelerated depth trajectory and eligibility in `claw_con1`; disallows initiating during clearance | Fixed .85-second sine trajectory, three-second cooldown, and allows jumping during clearance. This supports the requested XR spike-clearing interaction. |
| Campaign ending | `sweb0` completion/game-end path, source high-score/keys and Beastly behavior | Stops after prototype wave 100 with a victory screen. Source `sweb0` tests `cwave>=99` before starting another wave; this end threshold is not reproduced directly. No Beastly replay, source finish presentation or key awards. |

## Bonus comparison

Ring and psychedelic XY courses, packed delays/types, eight-course sequences, delay override, rectangular hit test, accuracy scoring and one-eighth boosts are imported or translated. Ring hits use 250/500/750; psycho preserves the unusual source lookup order. Vertical impulses, controls, speed integration, failure/victory scene duration and feedback are adapted. The source ring victory presentation lasts much longer and uses a pyramid/feedback scene; the current result holds for two seconds before the XR return.

The tunnel now translates `_tunn`/`tunrun`/`tundraw`: a 64-segment circular
buffer, two pattern generators, inertial rotation, quantized green-track contact
and a growing/shrinking trail. On-track frames score 1/4/8/30 by trail length;
off-track frames lose trail before failure. All eight courses complete through
public steering in 1,024 source frames, 64 turns and 29,662 points. There is no
20,000-point tunnel completion award. Trail crystals, patterned walls and the
33-frame collapse are rendered with modern geometry/colors. Mouse gain, XR
projection, entry and return remain adaptations. See step five below for the
source's unused speed-ramp counter quirk.

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
| 2 | `run_flipper` | Straight climb / rim turn / stopped states; remaining subtype gaps above |
| 3 | `run_zap` | Adapted explosion/flash |
| 4 | `kill_shot` | Shot removal |
| 5 | `run_tanker` | Three content families, rim/hit opening and source sound pair |
| 6 | `run_spike` | Independent spikes/super spikes and source shortening/deletion rules |
| 7 | `run_spiker` | Lane selection, build/retreat counters and one-spiker limit |
| 8 | `run_ashot` | EnemyShot; shared fire counter/cap, swept collisions |
| 9 | `run_fuseball` | Rail ride/crossing states, pursuit and count-based vulnerability |
| 10 | `blowaway` | Generic particle lifecycle |
| 11 | `run_pulsar` | 16-phase counter, phase-7 hazard and source climb speed |
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
| 23 | `run_pspark` | Contact-first rim sparks, timed propagation and edge bounce/wrap |
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
| 36 | `rumirr` | Shot takeover, knockback and retry state |
| 37 | `refsht` | Uninterceptable full-speed mirror ring; retains player shot slot |
| 38 | `refsht2` | Half-speed spinning beast part, absorbs normal shots and survives |
| 39 | `run_adroid` | Four states: shielded climb, start/cross patrol and zap |
| 40 | `loiter` | Respawn countdown; separate source multiplayer reserve-life behavior absent |

All twelve `inits` entries map to a Kind: flipper, tanker, spiker, fuseball, pulsar, fuse tanker, pulsar tanker, super flipper, mirror, hostile droid, beast, second super flipper. The secondary enemy dispatches now have explicit state translations and dedicated regressions. Pickup/frontend/effect dispatches still include the omissions described above. Importing generator types alone is not proof of state-machine equivalence.

## Drawing, special effects and frontend

All 18 `draw_vex` entries were classified: `rrts`, `draw`, `draw_z`, `draw_vxc`, `draw_spike`, `draw_pixex`, `draw_mpixex`, `draw_oneup`, `draw_pel`, `changex`, `draw_pring`, `draw_prex`, `dxshot`, `drawsphere`, `draw_fw`, `dmpix`, `dsclaw`, `dsclaw2`. Modern meshes, lines, bitmap particles, beam rings and death scenes cover their visible families. Distinct source transform modes, lifecycle counters, firework sequences and multiplayer claw variants are not fully ported.

The 27 `solids` slots include no-op, flipper, three tankers, shot, fuseball, pulsar, ring bullets, pixel explosions, pickups, gates, H2H claw/shot/generator/ball, mirror, ring-shot impact, blue flipper, two super flippers, three beast stages and hostile droid. Original imported meshes represent the campaign forms. H2H logic is absent despite some associated shapes being present in the data.

| System | Present | Remaining difference / omission |
|---|---|---|
| Polygon drawing (`obj2d.s`, `horse.gas`) | Original face topology and vertex intensity, animated mesh frames | Modern RGB lighting/blending; source projected orientation, CRY shading, clipping and animation counters differ |
| Raster/font drawing (`afont`, `bfont`, `cfont`, `images`) | Five imported sheets, all three glyph maps; title, messages, scores, One Up, Game Over | Modern layout and glyph fallback; source scaling, timing, palettes and every frontend graphic use are not reproduced |
| Starfield (`llama.gas`, `goat.gas`; `ringstars`, `siney`) | Full-sphere XR stars, ring/sine patterns, streaks | Spherical distribution and modern animation replace the source projected field buffers |
| Mode 7 (`ox.gas:mode7`) | Textured ring/psycho floor using original art | No source GPU raster projection, scanline arithmetic or exact ground animation |
| Tunnel (`ox.gas:starseg`, `voo`; `run_pgens`) | Translated pattern generators/buffer, green track and trail crystals | Modern wall projection, RGB conversion, crystal geometry and entry/return camera |
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

Eleven CTest suites now cover wave clearance, retained enemies, single-hit super
flippers, contact-first sparks, tanker emergence, all eight original tunnel
courses and the remaining enemy state machines. The 24-course suite uses public
controls. Separate regressions cover strict timed rim impacts, droid lifecycle,
bonus ordering/cameras, music selection, sample pitch/loops and sample-only voices.
These tests check translated behavior; they do not establish cycle/pixel identity.

Remaining priorities: source object-pool/backpressure and friendly-droid
movement/entitlement; rare pickups and score/start/one-up states; individual
explosion/firework lifecycles, ring victory scenes and music fade; then ending,
Beastly and frontend flows. Modern XR geometry/input/camera and the 120 Hz swept
collision framework remain intentional adaptations. The historical re-evaluation
below records the pre-fix observations; the ordered progress section records
their resolution.

## Rim-combat follow-up

`tests/rim-combat-test.cpp` reproduces the original reports and verifies public
fire-button timing, early/departed-shot misses, hits at the 52-unit boundary,
wrapped windows on concave webs, vacated-lane capture, pause, jump protection,
mode-2 rotation speed, landing pauses/chase direction, and smooth endpoints on
all campaign webs. Both orders of the rim/behind-enemy reproduction now give
the same result with normal, friendly and particle-laser shots. Opposing bullets
and spike occlusion use the same impact ordering. A pre-turn kill drops its
pickup in the original lane.

The source `flipcollie` compares the **signed, wrapped** target-angle distance
with 52. Usually this opens toward the end of a turn; some concave-web turns
start inside the wrapped window, leave it, then reopen it. A projectile must
actually overlap during that window. Merely having fired earlier is insufficient.
`rail`/`stdstop` check bullets before starting the next turn, so an existing shot
can still kill in the lane being left. A completed turn does not capture in the
destination until the stopped state runs.

The new animation rotates the centre around the shared rim vertex and blends
unequal lane radii to keep endpoints continuous. This is a geometric adaptation
of the source pivot; original integer projection and polygon rasterization are
not reproduced. Collision windows use the imported source orientation words
rather than approximating their values from modern geometry. The impact solver
is a swept modern implementation rather than the source linked-list update
order; it intentionally removes storage-order-dependent shot stealing.

## Remaining-implementation re-evaluation at f147c02

**Historical observations:** the six ordered implementation items below are now
implemented. The probes and line numbers in this section describe `f147c02`.
See the current comparison and ordered implementation progress for their fixes.

At that revision, all eight CTest suites passed. A separate controlled gameplay probe reproduced
the following missing or incorrect source behaviors. No gameplay code was changed
during this evaluation. Observed values use the public Game state and fixed
120 Hz ticks; they are not observations from a Jaguar emulator.

| Priority | Gap / reproduction | Observed current behavior | Source-required behavior / next implementation |
|---|---|---|---|
| P1 | Generation finished, sole flipper already at rim, player protected for observation | Remains Playing; clearance begins after **24.0083 s**, when the flipper expires | `r_end` compares the furthest counted enemy with `webz-80`, then calls `szoom`. Implement the depth-based clearance trigger and remove expiry as a substitute. Current condition: `src/game.cpp:1136`. |
| P1 | One valid normal hit on a climbing `SuperFlipper` / `SuperFlipper2` | Enemy survives with HP 1 / 2 and creates an EnemyShot | `railopts` selects `stdrail`; its collision path, `flipcollie` and `stdstop` branch to `fkillme` after one valid hit (11083–11085, 11137–11146, 11292–11294). Implement super movement variants without invented shield HP/ricochet. Keep beast/mirror reflection as separate source rules. Current hit handling: `src/game.cpp:1049`. |
| P1 | Spark reaches an open-web end | Spark count becomes zero | `run_pspark` reverses direction and continues inward. Preserve the spark and implement the source propagation counter. Current edge branch: `src/game.cpp:916`. |
| P1 | Spark and player share rim lane while a fresh shot is fired | Player keeps all three lives; spark dies for +200 | `run_pspark` checks `checlane` before `collie`. Contact must win here, unlike the stopped-flipper shot-before-capture rule. Current global impact pass precedes spark contact: `src/game.cpp:1093`. |
| P1 | Unshot tanker at depth .22, still well below rim | Tanker opens into two flippers | `run_tanker` opens on a valid hit or at `webz-80`. Move the automatic opening threshold to the rim and port child-entry states / source sound pair. Current threshold: `src/game.cpp:1100`. |
| P1 | Tunnel bonus rules | Radial opening targets, immediate failure on miss, fixed per-opening awards | Implement `_tunn` / `tunrun` / `tundraw`: green track, trail growth/loss, recoverable off-track tolerance, continuous scoring, rotating segmented walls and source difficulty progression. Imported turn bytes alone do not implement this game. |
| P1 | Enemy behavior completeness | Basic rim flippers now have their correct shared movement/window states; other families still share simplified generic fields | Port hostile-droid four-state AI, spiker lane/build selection and super spikes, fuseball crossing states/windows, exact pulsar phases, mode-3 down-web turns, tanker-emergence chains and beast/mirror projectile transformations. |
| P2 | Combat budgets and progression balance | Per-enemy firing timers, 64-entity gate, generic reflected EnemyShot, fixed player/droid cadence | Source object/shot pools, global alien firing counter, subtype limits, distinct reflected-shot speed/interception and source spawn backpressure are absent. |
| P2 | Score / pickup / ending flows | Fixed per-kind scores and web-clear reward, fixed point pickups, modern wave-100 victory | Port source score contexts, selected-start-level awards, rare surprise/infinite-zap/early-exit branch, one-up color state, exact bonus result flow and source completion/Beastly handoff. |
| P2 | Special-effect/audio completeness | Original art/meshes/PCM and modern feedback are present | Source firework command sequence, tunnel pattern/trail generators, bonus victory presentation and remaining per-effect lifecycles/color timing are incomplete. Music still lacks the 50-frame fade-down/wait; several explosion/impact contexts share a generic cue. |
| P3 | Original frontend and optional modes | Modern title, best score, unlocked web and preferences | Attract/tutorial/credits cycle, initials/high-score table, keys/passwords, full original options, permanent droid-start option, Classic/Plus/co-op/H2H are absent. XR controller bindings and physical headset/WiVRn validation are also outstanding; wheel-based XR is implemented. |

**Corrected audit finding:** the quiet-phase spark probe reports `pulse=0`,
`lives=2`, spark still present. Thus contact lethality outside pulses already
works. The remaining spark failures are edge handling, source contact priority
and exact propagation/pulse integration.

**Test-coverage gap:** `tests/campaign-test.cpp:190` currently groups super
flippers with mirrors and asserts survival plus a reflected bullet. That test
protects the prototype's incorrect super-flipper behavior and must change with
the fix. Campaign budget tests erase generated enemies, so they do not test the
source rim-based wave-clear condition. Completing all 24 prototype bonus
courses does not validate the missing original tunnel trail/track rules.

The labeled `raw_webs` table was checked against the generated web-order array;
it matches. The different table embedded earlier under `romstart` is not the
labeled runtime table used by `sweb`. Original sample-only audio, full-sphere
XR stars, ship-following camera and the repaired basic rim timing are already
present and are not counted as missing implementations.

## Ordered implementation progress

1. **Wave clearance implemented.** Generation completion now starts clearance
   once every counted enemy/alien bullet is at or above the rim. Pickups and
   spikes do not hold the wave open. Rim enemies survive into the flight; the
   artificial 24-second expiry was removed. `original-behavior-regressions`
   covers live rim enemies, down-web blockers, pickups and non-expiring enemies.
   Earlier reproduced gaps above describe the evaluated revision, not this fix.

2. **Super-flipper damage implemented.** Both variants die on one valid normal,
   friendly or laser hit and award the source flipper score. Invented armor,
   reflected bullets and health pips were removed. Mirror/beast rules remain
   separate. Campaign assertions now check the corrected behavior.

3. **Rim sparks implemented.** Contact precedes shots, open ends bounce, closed
   webs wrap, and propagation uses the source five-frame counter. Splitting
   pulsars place both sparks on the original lane with opposite directions.
   Quiet-phase lethality, same-tick contact, jump protection and edge timing
   have regressions; spark shot score follows `fkillme`.

4. **Tanker opening implemented.** All three families open only on a valid hit
   or at the rim. Flipper/pulsar children start pivoted turns at the parent's
   depth, then resume climbing/their own handler; they cannot capture the old
   lane during emergence. The original paired sample-17 pitch-60/61 cue replaces
   the generic explosion. Tests cover early non-opening, rim release and
   mid-web child turns. Fuseball crossing refinements are part of step six.

5. **Original tunnel rules implemented.** A 64-segment circular buffer and the
   source pattern generators feed the green track. Rotary inertia, quantized
   track contact, trail growth/loss, continuous tiered scoring and delayed
   collapse replace radial gates and instant failure. All eight courses finish
   through public steering controls in 1,024 source frames for 29,662 points.
   There is no invented 20,000-point tunnel completion award. Desktop and XR
   explain rotation speed, braking and trail recovery. Renderer checks and
   `original-tunnel` cover these rules, failure, recovery, pause and boundaries.
   Geometry, RGB wall colors and mouse gain remain modern adaptations.

   **Correction to the earlier speed finding:** `tundraw` loads `pongyv` into
   d6, but `inst` clears d6 before the final `dbra d6,inst`. The preserved active
   code therefore emits one segment per frame; its unused ramp variables do not
   accelerate the tunnel. The translation follows that executable path.

6. **Remaining enemy AI implemented.** Explicit spiker selection/build/retreat,
   super spikes, fuseball ride/crossing windows, hostile-droid four-state travel
   and shields, phase-7 pulsars, down-web mode-3 turns, tanker mutations/chains
   and beast/mirror projectile transformations replace generic behavior. Shared
   alien-fire counts/caps, source shot speeds/cadence and active-shot ceilings
   are translated. Reflections retain their slots, use distinct interception
   rules and do not block clearance. Jumping fire starts at the ship's depth.
   `original-enemy-ai` covers state boundaries, shielding, crossing counts,
   spike hit/deletion awards, reflection lifetime/speed, source zap immunity and
   single/shared firing counters. The renderer shows remaining beast parts,
   original mirror-ring raster/sample-19 cue, source pulsar cycle and super-spike flashing.

   Full source object allocation/reservations, friendly-droid patrol and separate
   shot entitlement remain outstanding. The existing 120 Hz swept impact
   framework remains an adaptation of the Jaguar's object-list update ordering.
