#pragma once
#include "game.hpp"
#include "music.hpp"
#include <SDL.h>
#include <array>
namespace tempest {
class Audio {
    friend struct AudioProbe;
    struct Voice {
        float phase = 0, remaining = 0, duration = 0;
        Sound sound = Sound::Shot;
        int sample = -1;
        double position = 0;
        float step = 1, gain = 1, pan = 0, delay = 0;
        int priority = 0;
        uint64_t serial = 0;
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
    int gatePeriod = 512;
    uint32_t random = 2000;
    uint64_t voiceSerial = 0;
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
