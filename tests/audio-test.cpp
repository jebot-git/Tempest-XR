#include "audio.hpp"
#include "sample_data.hpp"
#include <iostream>
#include <stdexcept>

using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(#c);                                                          \
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
        for (int i = 0; i < 32; ++i)
            audio.play(Sound::Wave);
        audio.play(Sound::Shot);
        CHECK(std::all_of(audio.voices.begin(), audio.voices.end(),
                          [](auto &v) { return v.sound == Sound::Wave; }));
        audio.play(Sound::OneUp);
        CHECK(std::none_of(audio.voices.begin(), audio.voices.end(),
                           [](auto &v) { return v.sound == Sound::OneUp; }));
        audio.play(Sound::Warp);
        CHECK(std::count_if(audio.voices.begin(), audio.voices.end(),
                            [](auto &v) { return v.sample == 9 && v.priority == 400; }) == 2);
        CHECK(std::count_if(audio.voices.begin(), audio.voices.end(),
                            [](auto &v) { return v.sample == 23 && v.priority == 101; }) == 2);
        CHECK(std::all_of(audio.voices.begin(), audio.voices.end(),
                          [](auto &v) { return v.remaining > 0 && v.priority >= 100; }));
        audio.voices = {};
        audio.play(Sound::Excellent);
        auto &left = audio.voices[0], &right = audio.voices[1];
        CHECK(std::abs(left.step - float(sourceSamples[21].period) / 352) < .0001f);
        CHECK(std::abs(right.step - float(sourceSamples[21].period) / 354) < .0001f);
        CHECK(left.pan < 0 && right.pan > 0 && left.priority == 101);
        std::vector<float> pcm(4800 * 2);
        Audio::callback(&audio, reinterpret_cast<Uint8 *>(pcm.data()),
                        int(pcm.size() * sizeof(float)));
        CHECK(left.position > 4800 && right.position > 4800);
        CHECK(std::all_of(pcm.begin(), pcm.end(),
                          [](float v) { return std::isfinite(v) && v >= -1 && v <= 1; }));
        double before = left.position;
        audio.suspended = true;
        Audio::callback(&audio, reinterpret_cast<Uint8 *>(pcm.data()),
                        int(pcm.size() * sizeof(float)));
        CHECK(left.position == before);
        CHECK(std::all_of(pcm.begin(), pcm.end(), [](float v) { return v == 0; }));
        audio.suspended = false;
        audio.voices = {};
        audio.play(Sound::OneUp);
        CHECK(audio.voices[1].delay > .07f);
        Audio::callback(&audio, reinterpret_cast<Uint8 *>(pcm.data()), 1000 * 2 * sizeof(float));
        CHECK(audio.voices[0].position > 0 && audio.voices[1].position == 0);
        Audio::callback(&audio, reinterpret_cast<Uint8 *>(pcm.data()), 4000 * 2 * sizeof(float));
        CHECK(audio.voices[1].position > 0);
        audio.voices = {};
        Game game;
        game.startBonus(0);
        game.bonusBoosts = 8;
        audio.updateMusic(game);
        audio.play(Sound::GateYes);
        CHECK(std::abs(audio.voices[0].step - 512.f / 552) < .0001f);
        CHECK(audio.voices[0].priority == 100);
    }
};
} // namespace tempest
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        CHECK(SDL_Init(SDL_INIT_AUDIO) == 0);
        AudioProbe::run(argv[1]);
        SDL_Quit();
        std::cout << "Original sample decoding, pitch, stereo voices, priority stealing, echo and "
                     "pause passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
