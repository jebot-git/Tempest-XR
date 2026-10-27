#include "audio.hpp"
#include "sample_data.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #c);                                                      \
    } while (false)
namespace tempest {
struct AudioProbe {
    static void run(const std::string &directory) {
        Audio audio;
        CHECK(audio.device != 0);
        SDL_PauseAudioDevice(audio.device, 1);
        audio.setSampleDirectory(directory);
        for (const auto &pcm : audio.effectSamples)
            CHECK(!pcm.empty());
        // Regression: voice 22 must last ~2.25 seconds, not the old 6.26 seconds.
        CHECK(std::abs(audio.effectSamples[22].size() / 96000.0 - 2.246) < .02);
        CHECK(effectStep(512) == 333);
        Game game;
        audio.setMusicDirectory(
            (std::filesystem::path(directory).parent_path() / "music").string());
        audio.updateMusic(game);
        CHECK(audio.currentTrack == "tune13.mod");
        CHECK(audio.effectChannels == 2);
        audio.play(Sound::Wave);
        for (int i = 0; i < 40; ++i)
            audio.play(Sound::Shot);
        CHECK(audio.voices[0].sound == Sound::Wave && audio.voices[1].sound == Sound::Wave);
        CHECK(audio.voices[2].remaining == 0);
        audio.play(Sound::Warp);
        CHECK(audio.voices[0].sample == 9 && audio.voices[1].priority == 400);
        audio.voices = {};
        audio.play(Sound::Excellent);
        auto &left = audio.voices[0], &right = audio.voices[1];
        CHECK(std::abs(left.step - float(effectStep(352)) / effectStep(512)) < .0001f);
        CHECK(right.step > left.step); // Pitch word is a phase increment, not inverse period.
        CHECK(left.pan < 0 && right.pan > 0 && left.priority == 101);
        std::vector<float> pcm(4800 * 2);
        auto render = [&] {
            Audio::callback(&audio, reinterpret_cast<Uint8 *>(pcm.data()),
                            int(pcm.size() * sizeof(float)));
        };
        audio.setVolumes(0, 0, 1);
        render();
        double energy = 0;
        for (float value : pcm) {
            CHECK(std::isfinite(value) && std::abs(value) <= 1);
            energy += value * value;
        }
        CHECK(energy > .1); // Recorded speech has its own audible bus, even with SFX off.
        double before = left.position;
        audio.suspended = true;
        render();
        CHECK(left.position == before);
        CHECK(std::all_of(pcm.begin(), pcm.end(), [](float v) { return v == 0; }));
        audio.suspended = false;
        audio.voices = {};
        audio.play(Sound::FlipperLand);
        CHECK(audio.voices[0].sample == 28 && audio.voices[0].priority == 2);
        audio.voices = {};
        audio.play(Sound::TankerOpen);
        CHECK(audio.voices[0].sample == 17 && audio.voices[1].sample == 17);
        CHECK(audio.voices[0].priority == 3);
        CHECK(std::abs(audio.voices[0].step -
                       float(effectStep(60)) / effectStep(sourceSamples[17].pitch)) < .0001f);
        audio.voices = {};
        audio.play(Sound::OneUp);
        CHECK(audio.voices[0].remaining > 0 && audio.voices[1].remaining == 0);
        audio.voices = {};
        audio.play(Sound::Yes);
        for (int i = 0; i < 40; ++i)
            render();
        CHECK(left.remaining > 0 && left.position < left.loopEnd &&
              left.position >= left.loopStart);
        game.start();
        audio.updateMusic(game);
        CHECK(left.remaining == 0); // Clearance loop ends with the outgoing mode.
        CHECK(audio.currentTrack == "rave4.mod");
        game.start(17);
        audio.updateMusic(game);
        CHECK(audio.currentTrack == "tune7.mod");
        audio.nextTrack();
        audio.updateMusic(game);
        CHECK(audio.manuallySelected);
        game.pause();
        audio.updateMusic(game);
        CHECK(audio.manuallySelected && audio.suspended);
        game.pause();
        game.start(18);
        audio.updateMusic(game);
        CHECK(!audio.manuallySelected && audio.currentTrack == "tune7.mod");
        game.startBonus(0);
        audio.updateMusic(game);
        CHECK(audio.currentTrack == "tune1.mod");
        game.startBonus(1);
        audio.updateMusic(game);
        CHECK(audio.currentTrack == "tune3.mod");
        game.start(33);
        audio.updateMusic(game);
        CHECK(audio.currentTrack == "tune5.mod");
        audio.voices = {};
        audio.play(Sound::ClearanceEngine);
        CHECK(left.loop && right.loop && left.sample == 0 && right.sample == 2);
        game.startBonus(0);
        game.bonusBoosts = 8;
        audio.updateMusic(game);
        audio.play(Sound::GateYes);
        CHECK(std::abs(left.step - float(effectStep(552)) / effectStep(512)) < .0001f);
        CHECK(left.priority == 100);
        audio.voices = {};
        audio.effectSamples[22].clear();
        audio.play(Sound::Wave);
        CHECK(left.remaining == 0); // Never synthesize a replacement announcer.
        audio.setMusicDirectory("/missing-tempest-tracks");
        game.start(49);
        audio.updateMusic(game);
        CHECK(audio.currentTrack == "tune1.mod" && audio.failedTrack == "tune12.mod");
        CHECK(audio.retryMusicAt > 0);
        SDL_CloseAudioDevice(audio.device);
        audio.device = 0;
        Audio noMusic;
        SDL_PauseAudioDevice(noMusic.device, 1);
        noMusic.setSampleDirectory(directory);
        noMusic.updateMusic(game);
        CHECK(noMusic.effectChannels == 6);
        for (int i = 0; i < 6; ++i)
            noMusic.play(Sound::Shot);
        CHECK(std::all_of(noMusic.voices.begin(), noMusic.voices.end(),
                          [](auto &v) { return v.remaining > 0; }));
    }
};
} // namespace tempest
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        CHECK(SDL_Init(SDL_INIT_AUDIO) == 0);
        AudioProbe::run(argv[1]);
        SDL_Quit();
        std::cout << "Original PCM rates, positive pitch, 2/6 channels, loop cancellation, "
                     "recorded voices, music contexts and pause passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
