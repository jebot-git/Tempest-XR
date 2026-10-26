# Third-party notices

Tempest 2000 is the 1994 game by Jeff Minter, published by Atari. Tempest and
Tempest 2000 names belong to their respective rights holders. This is an
unofficial recreation prototype/source study, with no implied endorsement.

`src/webs.hpp` contains extracted web coordinates, topology and starting lanes
from the preserved Jaguar source at
https://github.com/mwenge/tempest2k/tree/8b50b467ce7dd7eee475aaf5ae86804d95401ae7
and the level order from its `raw_webs` table. Credit for the original data is
Jeff Minter / Atari; credit for source preservation and annotations is the
upstream project's contributors. That repository does not provide a general
open-source license grant. The imported data is not relicensed by this project.

SDL2, Mesa/OpenGL/EGL, X11 and the OpenXR loader/runtime are system dependencies
and retain their respective licenses. No copies of their libraries or original
Tempest raster graphics are bundled here; original audio assets are described below.

`assets/music/*.mod` contains the seven original Jaguar Tempest 2000 music
modules from the same pinned repository's `src/sounds/` directory. Music credits:
Imagitec Design; Ian Howe, Alastair Lindsay, Kevin Saville, Julian Hodgson;
Atari Corporation. Original asset rights remain with their respective owners;
the modules are not relicensed by this prototype. `assets/music/manifest.json`
records source URLs and SHA256 checksums. These are the original in-game tracker
modules. Public availability of the source archive is not a new license grant.

Optional libopenmpt playback dynamically uses the system library under its BSD
license. Its stable C API is documented at https://lib.openmpt.org/doc/ .

`src/campaign.hpp` imports original wave schedules, tuning tables and bonus
course layouts from `yak.s`. `src/meshes.hpp` imports original polygon vertices,
face lists and intensity values from `obj2d.s`. Their generated headers record
source URLs and SHA256. These original data retain their original attribution
and are not relicensed by this project.

`assets/sfx/*.wav` are lossless PCM conversions of the 30 original effect/voice
payloads in the pinned repository's `src/sounds/samples/` directory, using its
sample metadata. Original sound/voice rights remain with their respective
owners. The effect manifest records raw payload hashes, source URLs and nominal
rates. This repository does not assert a new license grant for original assets.

The test-only OpenXR API fixture in `tests/mock-openxr.cpp` adapts the de-xr
Comic Chat mock runtime, under the MIT license reproduced in
`tests/MOCK_RUNTIME_LICENSE.txt`. It is not installed with the game. The native
DomeD link uses the system libcurl and nlohmann-json libraries; no copies of
those libraries are bundled by this project's installer.
