# Validation

Validated on Linux, 2026-10-26, with SDL 2.32.10, Mesa 26.0.8 and Intel
ADL-N graphics. Physical headset/WiVRn validation remains outstanding.

Both the default OpenXR build and `TEMPEST_OPENXR=OFF` desktop build pass all
five CTest suites:

- Gameplay, steering, transitions and ship-follow camera.
- Audit regressions, enemy roster, all 100 wave budgets, upgrades, both zaps,
  bonus success/failure and campaign victory.
- Original tracker decoding, module selection and pause behavior.
- OpenGL sky coverage, source meshes, enemies, bonus scenes and effects.
- Original module hashes and lossless sample conversion.

The campaign regression executable also passes AddressSanitizer and
UndefinedBehaviorSanitizer. Regenerating webs, campaign tables and meshes from
the pinned source reproduces their generated headers byte for byte.

The isolated native OpenXR fixture (`python3 tools/test_xr.py`) used Monado's
simulated HMD, null compositor and native EGL:

| Scenario | Frames | Stereo submissions |
|---|---:|---:|
| Effects, jump and tunnel flight | 400 | 399 |
| Enemy roster | 90 | 89 |
| Ring bonus | 90 | 89 |
| Tunnel bonus | 90 | 89 |
| Psychedelic bonus | 90 | 89 |

The first session exercised 144 transition frames, reached 13.9995 meters of
the 14-meter flight, and included 102 jumping frames with a late jump across
the web reset. The first frame in each session precedes stereo submission.

An isolated Xvfb input check opened normal play, started the saved level and
quit through Ctrl+Q. The six-field settings format restored unlocked wave 17
and its `tune7.mod` selection. The older five-field format restored wave 1 and
`rave4.mod`. Both retained the best score and wrote the six-field format via
the temporary-file rename path. Normal user preferences were untouched.

These checks establish simulated runtime integration and rendering, rather
than physical tracking, headset comfort or wireless streaming performance.
