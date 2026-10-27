// Generated from the original sound_samples_table.bin; Jeff Minter / Atari.
// https://raw.githubusercontent.com/mwenge/tempest2k/8b50b467ce7dd7eee475aaf5ae86804d95401ae7/src/incbin/sound_samples_table.bin
#pragma once
#include <array>
namespace tempest {
// Included Imagitec driver: SCLK=27, positive pitch word, 8.8 phase.
constexpr int effectStep(int pitch) { return pitch*28/43; }
struct SampleSpec {int pitch,priority,loopStart,loopEnd;};
inline constexpr std::array<SampleSpec,30> sourceSamples={{
{428,1,2,4512},
{428,2,0,0},
{428,3,3086,12706},
{214,4,0,0},
{428,5,0,0},
{428,6,0,0},
{428,7,0,0},
{214,8,0,0},
{428,9,0,0},
{568,10,0,0},
{428,11,0,0},
{428,12,0,0},
{428,13,0,0},
{254,14,0,0},
{428,15,0,0},
{856,16,0,0},
{214,17,0,0},
{856,18,0,0},
{284,19,0,0},
{856,20,0,0},
{170,21,0,0},
{512,22,0,0},
{512,22,0,0},
{512,24,2,23148},
{512,25,0,0},
{512,26,0,0},
{512,27,0,0},
{512,28,0,0},
{512,30,0,0},
{512,31,0,0},
}};
}
