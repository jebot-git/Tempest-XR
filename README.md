# Tempest XR

A source-informed **Tempest 2000** recreation for Linux desktop and native
OpenXR, with mouse-wheel controls, a ship-following XR camera, the original
Jaguar music and sound samples, original shaded meshes and raster artwork/fonts, and the imported Jaguar campaign data.

Detailed fidelity status and remaining work: [behavior audit](docs/BEHAVIOR_AUDIT.md).

Reference: [Jeff Minter's Jaguar source, preserved by mwenge](https://github.com/mwenge/tempest2k),
pinned to `8b50b467ce7dd7eee475aaf5ae86804d95401ae7`. This is a native C++17,
SDL2 and OpenGL implementation; no ROM, emulator or Jaguar toolchain is needed.

## Build and run

Requires CMake 3.18+, a C++17 compiler, pkg-config, SDL2, OpenGL/EGL, X11,
libcurl, nlohmann-json and the OpenXR loader development packages. On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libsdl2-dev libgl-dev \
  libegl-dev libx11-dev libopenxr-dev libcurl4-openssl-dev nlohmann-json3-dev libopenmpt0
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
./build/tempest-xr
```

`libopenmpt0` is optional for tracker music; its development headers are
unnecessary. A desktop build can omit OpenXR with `-DTEMPEST_OPENXR=OFF`.
Configure `-DCMAKE_INSTALL_PREFIX=PREFIX`, then install with `cmake --install build`.
The icon, desktop entry, music and effects install together. DomeD's default
bundled-experience staging builds a pinned Release revision of this project.

## Controls

Hold LMB to fire and use the wheel to steer. Fractional wheel input accumulates;
closed webs wrap, and open webs stop at their ends.

| Input | Action |
|---|---|
| Wheel | Move on rim; select an unlocked starting web on the title screen |
| Left mouse | Start in XR/SBS; hold to fire repeatedly |
| Right mouse / Z | Paced Superzapper: first use sweeps eligible enemies, second zaps one |
| Middle mouse / Space | Jump after collecting the jump powerup |
| Left/Right or A/D | Alternative steering / starting-web selection |
| Enter | Start/retry; save initials when entering a high score |
| K on title | Change saved player access key |
| O on title | Toggle Beastly replay after completing the campaign |
| P / Escape | Pause/resume; Escape returns game-over/victory to title |
| V | Enter native OpenXR / return the game to desktop |
| Alt+Tab | Pause native XR and restore the previous DomeD desktop |
| Click / Enter while paused | Resume play after returning to the game |
| R | Recenter the XR rig |
| B | Half-SBS preview using the XR camera |
| [ / ] | Wheel sensitivity, 0.25–4 lanes per detent |
| I | Reverse wheel direction |
| M | Mute music and effects |
| N | Preview soundtrack modules until the next web/bonus/title |
| F2/F3 | Music volume down/up |
| F4/F5 | Sound effects volume down/up |
| F6/F7 | Recorded voice volume down/up |
| F11 | Fullscreen |
| Ctrl+Q | Quit |

Normal firing requires holding LMB. Releasing it stops new shots. There is no
hands-free autofire toggle; older saved autofire preferences are ignored.

In ring and psychedelic bonus stages, the **wheel changes horizontal drift**;
reverse it to brake. **Hold LMB (original button A) to accelerate up** and
**RMB (button C) to accelerate down**. Releasing the buttons coasts with friction.
Up/W and Down/S are keyboard alternatives. In the tunnel bonus,
the wheel or Left/Right/A/D adjusts rotation speed; reverse to brake. Stay on
the green track to grow the trail and score continuously. Going off track
shrinks the trail; recover before it runs out. Instructions appear in desktop
and XR views. These adapt the original rotary controller to a mouse.

## Campaign

All 100 source `_nw` schedules are imported, including initial half-period
delays, repeat intervals and generation budgets. The source wave header is an
**enemy count**, not a duration: `newgen` decrements it after launching an
object. The original final-wave alias is preserved. Web selection follows
`16 + ((level - 1) & 31)` from the shared 48-entry table. Original speed,
pulse and powerup-delay tables drive progression. As in `sweb0`, clearing displayed
web 99 and completing its flight ends the campaign and unlocks Beastly mode. The
100th imported schedule remains available for diagnostics with `--level 100`.

The generator roster includes flippers, three tanker variants, spikers,
fuseballs, pulsars, two super-flipper variants, mirrors, hostile droids
and beasts. Tankers release their corresponding pairs. Pulsars electrify lanes only in phase seven and split into bouncing rim sparks.
Fuseballs pursue the ship through timed rail crossings and can be shot only
near the crossing center. Spikers choose clear/shortest spikes, build and
retreat; super spikes stop normal shots. Super flippers take one valid hit.
Mirrors convert shots into rings; beasts shed absorbing reflected parts.
Hostile droids deflect shots while climbing, then patrol above the rim and zap;
jump and fire to reach them. Enemy projectiles and a
separately acquired friendly firing droid are implemented. Flippers climb to
the rim before turning toward the ship. Their turning animation and shot
vulnerability share the source angle state: a correctly timed shot can kill
before capture, while an already departed shot misses. Hits resolve in impact
order so an enemy farther down the lane cannot steal a shot from a rim target.

Powerups progress through **particle laser → points → jump → points → friendly
droid → bonus token**, followed by additional point awards. The first killed
enemy offers a pickup after its source rez animation; subsequent drops use the imported per-wave kill delay.
Collected droids expire at web clearance. Catching a pickup during clearance schedules an early droid on the first pickup of the next web. Three tokens earn a bonus after the clearance flight. Ring, tunnel and psychedelic
families use the original course layouts/order. Packed gate delays control spacing;
up/down gates impart a vertical impulse, lightning gates increase speed by one
eighth, and pyramid gates finish the course. Ring accuracy awards 250/500/750
points using the source scoring rule. Psycho mode keeps its source delay override
and scoring rule. Ring/psycho completion awards 20,000 points. Successful bonuses warp five levels, with the source's
late-campaign limit on skipping. Failing a bonus costs no life. Successful ring/psycho courses show the
pyramid/feedback victory scene for 401 source frames. Point pickups have the
original rare early-exit/infinite-zap surprise. Clearance awards no flat bonus.

Beastly doubles scores, halves player-shot speed, slows player firing, removes
ordinary flipper pauses and permits eight alien bullets. The title screen shows
the original default high-score table, instructions and credits. Qualifying
scores accept three initials: wheel changes the current letter, LMB advances,
RMB goes back, and Enter saves; typing is also supported. K selects one of four
source access-key names, with independent unlocked webs and Beastly unlocks.
`records.json` stores the table and keys alongside the existing `settings` file.
Pause, Game Over, victory, title and HUD text appear directly over the scene.

Jump lasts 0.85 seconds with a three-second recharge. Enemy bullets can be
intercepted with normal, particle-laser or friendly-droid fire. Death removes
upgrades, freezes combat for a 1.6-second explosion/capture sequence, and restarts
the current wave with brief protection. The camera glides back to its starting
rim. A flipper capture carries the claw down the tunnel; other deaths scatter
its original polygon faces.
Spikes survive Superzapper, block shots, and remain hazardous throughout the
ship's passage along them during clearance. Every 20,000 points awards a life,
including thresholds crossed by wave/bonus awards. An airborne jump continues
across the next-web reset without a camera snap.

Unlocked starting webs, best score and settings live in SDL's preferences
directory (typically `~/.local/share/de-xr/tempest-xr/settings`). Progress saves
at new-web checkpoints, game over, victory and normal exit. Demo/capture and
explicit `--level` runs do not overwrite normal progress.

## XR

```sh
./build/tempest-xr --xr
XR_RUNTIME_JSON=/usr/share/openxr/1/openxr_wivrn.json ./build/tempest-xr --xr
./build/tempest-xr --stereo
./build/tempest-xr --xr --warp-demo
```

The camera follows the ship with a 3.8-meter standoff. Rim movement pans the rig,
jumps move it backward by up to 0.8 meters, and clearance flies it 14 meters
through the tunnel over 2.4 seconds. The next web is rendered beyond the old
one; the flight joins its starting lane before resetting the world origin.
OpenXR predicted eye poses/FOVs remain layered onto the moving rig. A full-sphere
background fills every head direction, with animated ring/sine patterns and warp
streaks. Bonus entry, bonus exit and the next-web reset preserve camera continuity.
The headset HUD shows zaps, jump readiness/recharge, tokens, upgrades, bonus
progress/speed, pause/resume instructions and campaign victory.

In DomeD, the validated launch socket determines the default mode: launch from
XR to enter native OpenXR immediately, or from desktop for desktop play.
`--desktop` and `--xr` explicitly override the launch realm. V transfers a running
desktop game into native XR and returns an XR game to a focused desktop window.

Native XR uses DomeD's reversible handoff API: the compositor hides its XR
desktop and both bars and routes keyboard, wheel and mouse buttons to the game,
independent of pointer location. Alt+Tab remains a system escape: gameplay and
audio pause, the game's OpenXR session closes, mouse capture releases, and the
previous bar visibility, desktop, workspace and focus return. Other windows keep
their geometry. Select Tempest XR in the taskbar to reopen its native XR view;
click or Enter resumes play. Exit, client death and a lost heartbeat also restore
the desktop. An older DomeD is rejected before starting native XR, so updating
the compositor requires a new DomeD session without restarting an active session
as part of installation.

Outside DomeD, ordinary desktop and explicit `--xr` modes remain available.
XR controller bindings are not implemented. The desktop companion shows an
overview; B selects a stereo camera preview. Runtime loss returns to desktop and
pauses. Runtime exceptions release mouse capture and the DomeD lease through
the same cleanup path as a normal XR exit. Wayland uses
EGL plus `XR_MNDX_egl_enable`; X11 can use GLX. Missing runtime/headset support
leaves desktop play available and prints the reason.

## Music, samples and effects

The seven original Jaguar `.mod` files are in `assets/music/`. Title, gameplay
16-wave blocks and bonus music use the original selection. Thirty original
sample payloads are converted losslessly to WAV for shots, explosions, jumps,
zaps, powered fire, ricochets and voice announcements. Sample playback uses
the included Imagitec driver's positive pitch conversion and loop metadata. The mixer
allows two effects alongside music, or six without it. Death and Excellent use
source detuned pairs; Recharge, gate, token and capture cues use paired samples.
Engine loops accompany clearance; a clearance pickup triggers the original Yes
loop. Ordinary arrival plays Warp, followed by Recharge after 80 source frames.
**No synthetic speech or synthesized effect fallback is used.** Missing samples
are silent; gameplay upgrade labels stay visual. Music, effects and recorded
voices have independent volumes (defaults 90%, 32%, 90%), persisted with settings.
F2/F3, F4/F5 and F6/F7 adjust them; paused desktop displays the levels and current
music. CLI equivalents are `--music-volume`, `--sfx-volume` and `--voice-volume`
with values 0–100. N previews a track until the next web or bonus/title context;
automatic gameplay music changes in 16-web blocks, matching the source.

Original `obj2d.s` face lists and vertex intensities supply 37 shaded/animated
meshes, including animated beast, tanker and jump frames. Five original CRY
sprite sheets are decoded using the Atari hardware color tables. All three
96-character Joby font maps are imported. The title logo, score graphics, 2000
points, One Up and Game Over use source pixels; announcements shatter those
pixels and font glyphs into the scene. Ring/psychedelic floors use the original
swirly background texture. Effects also include colored explosions, Superzapper
arcs, lane pulses, rainbow beam rings, ship trails and warp bridges/rings.
Melt-o-vision-style scale/rotate/intensity feedback keeps independent histories
for both XR eyes, SBS eyes and the desktop.

```sh
./build/tempest-xr --enemy-demo
./build/tempest-xr --bonus-demo 0       # 0 rings / 1 tunnel / 2 psychedelic
./build/tempest-xr --stereo --effects-demo
./build/tempest-xr --death-demo 0       # 0 explosion / 1 flipper capture
./build/tempest-xr --no-music          # Keep effects
./build/tempest-xr --music-dir /path/to/modules
```

`TEMPEST_MUSIC_DIR`, `TEMPEST_EFFECTS_DIR` and `TEMPEST_ART_DIR` override asset directories. Installed
assets live under `PREFIX/share/tempest-xr/`. Fetch/import tools pin the source
commit and record URLs/checksums; see [source notes](docs/SOURCE_NOTES.md) and
[third-party notices](THIRD_PARTY_NOTICES.md).

## Validation

```sh
ctest --test-dir build --output-on-failure
mkdir -p artifacts
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/tempest-xr --smoke-test artifacts/gameplay.ppm
python3 tools/test_xr.py              # Optional Monado simulated HMD + compatible GPU
```

Tests cover timed rim shots/capture, angular windows, impact ordering,
wave clearance, enemy state boundaries, shot budgets and all 100 generation
budgets, powerup/droid progression, both zaps, bonus success/failure and victory.
Tests complete all 24 bonus courses using public wheel/button controls, and
verify projectile interception, continuous bonus boundaries and death/respawn.
Render checks cover sky directions, source art/fonts, mesh animation, XR HUD
states, bonus modes and effects. Mixer tests cover pitch, sample-only voices, loop regions, two/six channels, music contexts, stereo pairs,
priority stealing and pause. Asset checks verify module/sample/art hashes.
Recorded results are in [validation](docs/VALIDATION.md).
The native XR fixture uses a private runtime directory and leaves the system
runtime unchanged. It exercises effects/late-jump warp, enemy roster and all
three bonus families, explosion and capture in actual OpenXR sessions. A separate
CTest injects an OpenXR runtime failure and checks capture release before exit.

DomeD handoff integration can be tested without connecting to a real headset:

```sh
python3 tools/test_domed.py --compositor ../DomeD/target/release/dome-compositor
```

This runs the real game in a private Wayland compositor with a test-only OpenXR
API runtime. It verifies launch modes, physical wheel capture, pause/restoration,
returning to XR, desktop toggling, client death, stopped-thread expiry and runtime
failure recovery. It requires an upstream DomeD build with the native handoff API.
`--report diagnostics.json` writes aggregate launch/pause/input/render results;
it does not log keys or lease tokens.

Useful flags: `--desktop`, `--level 1..100`, `--demo` (autoplay), `--frames N`,
`--capture image.ppm`, `--no-audio`, `--help`. Physical headset/WiVRn tracking,
streaming performance and comfort still require target-device validation.

This remains a source-informed recreation. Source countdowns, distant-pixel
entry, allocation/backpressure, independent shot ownership, friendly-droid
pursuit, rare pickups, effect lifecycles, music fade and ending/Beastly flows
have regression coverage. Collision ordering at 120 Hz, camera motion, mouse
gain, XR projection, RGB shading and feedback are adapted. Original
Classic/Plus/cooperative/head-to-head variants, full source option/attract
choreography and physical EEPROM encoding remain outside this implementation.
See the [current behavior audit](docs/BEHAVIOR_AUDIT.md) for precise limits.
