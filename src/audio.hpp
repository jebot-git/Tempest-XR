#pragma once
#include "game.hpp"
#include "music.hpp"
#include <SDL.h>
#include <array>
namespace tempest {
class Audio {
    struct Voice {
        float phase = 0, remaining = 0, duration = 0;
        Sound sound = Sound::Shot;
        int sample = -1;
        size_t position = 0;
    };
    SDL_AudioDeviceID device = 0;
    std::array<Voice, 32> voices{};
    std::array<std::vector<float>, 30> effectSamples;
    uint64_t samples = 0;
    bool muted = false;
    bool suspended = false;
    Mode audioMode = Mode::Menu;
    Music music;
    std::string musicDirectory, currentTrack;
    bool manuallySelected = false;
    int manualTrack = -1;
    static void callback(void *self, Uint8 *stream, int size);

  public:
    Audio();
    ~Audio();
    void play(Sound sound);
    void mute(bool value);
    void setMusicDirectory(const std::string &directory);
    void updateMusic(const Game &game, bool suspended = false);
    void nextTrack();
    void setSampleDirectory(const std::string &directory);
};
} // namespace tempest
