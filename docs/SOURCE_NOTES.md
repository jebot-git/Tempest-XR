# Source provenance and adaptation

All imports pin [mwenge/tempest2k](https://github.com/mwenge/tempest2k) commit
`8b50b467ce7dd7eee475aaf5ae86804d95401ae7`. Original game: Tempest 2000 (1994),
Jeff Minter / Atari. Source preservation/annotations: upstream contributors.

| Original data/routines | Implementation |
|---|---|
| `web1` … `web37`, `raw_webs`, `sweb` | Original outlines, topology and spawn lanes. Tempest 2000 selects index `16 + ((level-1)&31)` from the shared 48-entry table. |
| `_nw1` … `_nw100`, `setnewgen`, `newgen`, `inits` | All original generation budgets and independent spawn periods. First spawn waits half a period; each subsequent spawn waits a full period. |
| `fz_stuff`, `tz_stuff`, `sz_stuff`, `fuz_stuff`, `pudels`, `pup_stuff` | Original speed, pulse and pickup-delay tables. |
| `run_flipper`, `flipmodes`, `make_sflip2/3`, `make_beast` | Climbing, lane changes, rim movement, shield/ricochet and beast behaviors. |
| `maketank`, `openflippers`, `openfuseballs`, `openpulsars` | Three tanker families release the corresponding pairs. |
| `run_fuseball`, `climbrail`, `run_pulsar`, `run_pspark` | Rail pursuit, lane electrification and propagating rim sparks. |
| `rumirr`, `refsht`, `run_adroid`, `run_ashot` | Mirrors, reflected/projectile fire and hostile droid hazards. |
| `pupvex`, `hilaser`, `jenab`, `adenoid`, `sprzap`, `mkdroid` | Laser/points/jump/points/droid/token progression and friendly autonomous firing. Existing droids let the pickup sequence skip to the token. |
| `fire`, `r_end1`, `szap_avail` | Full initial Superzapper followed by one-enemy emergency zap. Spikes survive both. |
| `doscore`, `scores` | Centralized scoring and threshold awards, including wave and bonus score. |
| `bonies`, `courses`, `tunnels`, `cg`, `run_gate` | Three bonus families, original ring XY layouts and tunnel steering courses, token entry and five-level reward with late-game skip limit. |
| `modtable`, `webtunes`, `modnum` | Original seven tracker files; title, 16-wave gameplay blocks and bonus selections. |
| `obj2d.s` | Original face topology, per-vertex intensities and 37 shaded/animated object meshes. |
| `ringstars`, `initpstarfield` | Animated ring and sine pattern families extended to a spherical XR sky. |
| `ipix`, `any_pixex`, `draw_mpixex`, `draw_pring` | Colored explosion pixels, bitmap breakup, beam rings and trails. |
| `fade`, `ffade` | Per-view scale/rotate/intensity framebuffer feedback (source scale 500/512). |
| `czoom1/2`, `zoomup/down` | Wheel-following XR chase rig, jump recoil and continuous tunnel flight. |

## Import tools

```sh
python3 tools/import_webs.py
python3 tools/import_campaign.py
python3 tools/import_meshes.py
python3 tools/fetch_soundtrack.py
python3 tools/fetch_effects.py
python3 tools/check_assets.py
```

`import_webs` and `import_campaign` accept `--source /path/to/yak.s`;
`import_meshes` accepts `--source /path/to/obj2d.s`. The asset fetchers accept
`--source /path/to/tempest2k-checkout`. Without those options each reads the pinned
GitHub revision. Generated headers record the original URL/SHA256. Asset
manifests record source URLs, sizes and hashes. No ROM is used.

A web header stores the last vertex index. Closed webs therefore have one more
lane than open webs with the same number of vertices. The repeated vertex and
connection sentinel must not become extra lanes. Coordinates are recentered,
Y-flipped and scaled; the open plane is offset downward for visibility.

The wave header's original annotation says duration, but the executable
`launch1` code decrements it **per generated object** and stops below zero.
Consequently header 24 generates 25 objects. `_nw99` has its own table commented
out and aliases the following `_nw100` data; the importer preserves that behavior.
Spawn ticks use a 60 Hz reference, with a fixed 120 Hz simulation step.

Ring-course words pack delay, signed X, signed Y and gate effect. Their XY
layouts and eight-course order are imported. The prototype uses fixed approach
cadence and mouse-accessible hit windows. Tunnel courses preserve the original
64-step steering sequences. These are playable adaptations rather than a direct
port of the original mode-7 GPU renderer and every gate modifier.

The music files remain original module bytes. `modnum=1` is title, 3/4 are bonus,
and `webtunes` selects gameplay modules in 16-wave blocks (`lsr #4`, despite a
nearby annotation saying 32). libopenmpt is an optional system dependency.

Effects come from the signed 8-bit payloads in `sounds/samples`. The binary
metadata stores lengths as 24:8 fixed point. The fetcher produces lossless
16-bit PCM WAV and preserves each raw payload hash. Sample rates are nominal
period-derived values. Some preserved payload lengths differ from the old
metadata (for example sample 02); the manifest records both sizes. Playback
uses the available payload, never reads beyond it, and falls back to synthesis
if files are missing. Jaguar mixer pitch/echo/priority behavior is adapted.

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
