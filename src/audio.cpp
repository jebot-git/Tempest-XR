#include "audio.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>
namespace tempest {
Audio::Audio() {
    SDL_AudioSpec desired{}, actual{};
    desired.freq = 48000;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 512;
    desired.callback = callback;
    desired.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &desired, &actual, 0);
    if (device)
        SDL_PauseAudioDevice(device, 0);
    else
        std::cerr << "Audio unavailable: " << SDL_GetError() << '\n';
}
Audio::~Audio() {
    if (device)
        SDL_CloseAudioDevice(device);
}
void Audio::setSampleDirectory(const std::string &directory) {
    if (!device)
        return;
    SDL_LockAudioDevice(device);
    int loaded = 0;
    for (int i = 0; i < 30; i++) {
        char name[16];
        std::snprintf(name, sizeof(name), "%02d.wav", i + 1);
        auto path = (std::filesystem::path(directory) / name).string();
        SDL_AudioSpec spec{};
        Uint8 *bytes = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV(path.c_str(), &spec, &bytes, &length))
            continue;
        SDL_AudioCVT conversion{};
        int result = SDL_BuildAudioCVT(&conversion, spec.format, spec.channels, spec.freq,
                                       AUDIO_F32SYS, 2, 48000);
        if (result >= 0) {
            conversion.len = int(length);
            std::vector<Uint8> buffer(size_t(length) * size_t(std::max(1, conversion.len_mult)));
            std::copy_n(bytes, length, buffer.data());
            conversion.buf = buffer.data();
            if (SDL_ConvertAudio(&conversion) == 0) {
                size_t count = size_t(conversion.len_cvt) / sizeof(float);
                auto &samples = effectSamples[size_t(i)];
                samples.resize(count);
                std::memcpy(samples.data(), conversion.buf, count * sizeof(float));
                loaded++;
            }
        }
        SDL_FreeWAV(bytes);
    }
    SDL_UnlockAudioDevice(device);
    std::cerr << "Original effects: " << loaded << " samples\n";
}
static int effectSample(Sound sound) {
    switch (sound) {
    case Sound::Shot:
        return 5;
    case Sound::Kill:
        return 16;
    case Sound::Hit:
        return 3;
    case Sound::Jump:
        return 6;
    case Sound::Zap:
        return 7;
    case Sound::Wave:
        return 22;
    case Sound::Pickup:
        return 12;
    case Sound::Warp:
        return 9;
    case Sound::PoweredShot:
        return 11;
    case Sound::Excellent:
        return 21;
    case Sound::OneUp:
        return 24;
    case Sound::Pulse:
        return 19;
    case Sound::Ricochet:
        return 20;
    case Sound::GateYes:
        return 26;
    case Sound::GateYesAlt:
        return 27;
    }
    return -1;
}
void Audio::mute(bool value) {
    if (!device)
        return;
    SDL_LockAudioDevice(device);
    muted = value;
    SDL_UnlockAudioDevice(device);
}
void Audio::play(Sound sound) {
    if (!device)
        return;
    SDL_LockAudioDevice(device);
    for (auto &v : voices)
        if (v.remaining <= 0) {
            float length = sound == Sound::Shot   ? .045f
                           : sound == Sound::Zap  ? .6f
                           : sound == Sound::Wave ? .55f
                           : sound == Sound::Warp ? 2.4f
                                                  : .18f;
            v = {0, length, length, sound};
            int sample = effectSample(sound);
            if (sample >= 0 && !effectSamples[size_t(sample)].empty()) {
                v.sample = sample;
                v.remaining = float(effectSamples[size_t(sample)].size()) / (48000 * 2);
                v.duration = v.remaining;
            }
            break;
        }
    SDL_UnlockAudioDevice(device);
    if (sound == Sound::Warp) {
        SDL_LockAudioDevice(device);
        for (auto &v : voices)
            if (v.remaining <= 0 && !effectSamples[23].empty()) {
                v = {};
                v.sound = Sound::Warp;
                v.sample = 23;
                v.remaining = float(effectSamples[23].size()) / (48000 * 2);
                v.duration = v.remaining;
                break;
            }
        SDL_UnlockAudioDevice(device);
    }
}
void Audio::setMusicDirectory(const std::string &directory) {
    musicDirectory = directory;
    if (!directory.empty() && !music.available())
        std::cerr << "Soundtrack disabled: install the libopenmpt0 runtime\n";
}
void Audio::updateMusic(const Game &game, bool suspended) {
    if (!device)
        return;
    static const char *allTracks[] = {"tune13.mod", "tune7.mod", "tune1.mod", "tune3.mod",
                                      "rave4.mod",  "tune5.mod", "tune12.mod"};
    std::string desired = manuallySelected ? allTracks[manualTrack % 7]
                          : game.isBonus()
                              ? (game.bonusType == 1 ? "tune3.mod" : "tune1.mod")
                              : Music::trackForLevel(game.level, game.mode == Mode::Menu ||
                                                                     game.mode == Mode::GameOver ||
                                                                     game.mode == Mode::Victory);
    if (!musicDirectory.empty() && desired != currentTrack) {
        SDL_LockAudioDevice(device);
        bool loaded = music.load((std::filesystem::path(musicDirectory) / desired).string());
        SDL_UnlockAudioDevice(device);
        currentTrack = desired;
        std::cerr << (loaded ? "Soundtrack: " : "Soundtrack unavailable: ") << desired << '\n';
    }
    SDL_LockAudioDevice(device);
    if (game.mode != Mode::Paused && game.mode != audioMode) {
        for (auto &voice : voices)
            if (voice.sound == Sound::Warp)
                voice.remaining = 0;
        audioMode = game.mode;
    }
    this->suspended = suspended || game.mode == Mode::Paused;
    music.pause(suspended || game.mode == Mode::Paused);
    SDL_UnlockAudioDevice(device);
}
void Audio::nextTrack() {
    manuallySelected = true;
    manualTrack = (manualTrack + 1) % 7;
}
void Audio::callback(void *self, Uint8 *stream, int size) {
    auto &audio = *static_cast<Audio *>(self);
    float *out = reinterpret_cast<float *>(stream);
    int count = size / int(sizeof(float) * 2);
    if (audio.suspended) {
        std::fill_n(out, size_t(count) * 2, 0.f);
        return;
    }
    audio.music.render(out, size_t(count));
    for (int i = 0; i < count; i++) {
        float mixed = 0, left = 0, right = 0;
        for (auto &v : audio.voices)
            if (v.remaining > 0) {
                if (v.sample >= 0) {
                    auto &samples = audio.effectSamples[size_t(v.sample)];
                    if (v.position + 1 < samples.size()) {
                        left += samples[v.position] * .3f;
                        right += samples[v.position + 1] * .3f;
                        v.position += 2;
                    } else
                        v.remaining = 0;
                    v.remaining -= 1.f / 48000;
                    continue;
                }
                float u = v.remaining / v.duration, freq = 220;
                switch (v.sound) {
                case Sound::Shot:
                    freq = 600 + 1800 * u;
                    break;
                case Sound::Kill:
                    freq = 110 + 500 * u * u;
                    break;
                case Sound::Hit:
                    freq = 45 + 90 * u;
                    break;
                case Sound::Jump:
                    freq = 350 + 750 * (1 - u);
                    break;
                case Sound::Zap:
                    freq = 60 + 1800 * u * u;
                    break;
                case Sound::Wave:
                    freq = 220 * (1 + int((1 - u) * 4));
                    break;
                case Sound::Pickup:
                    freq = 660 + 660 * (1 - u);
                    break;
                case Sound::Warp:
                    freq = 55 + 1500 * (1 - u) * (1 - u);
                    break;
                case Sound::PoweredShot:
                    freq = 1000 + 2200 * u;
                    break;
                case Sound::Excellent:
                case Sound::OneUp:
                    freq = 440 + 880 * (1 - u);
                    break;
                case Sound::Pulse:
                    freq = 120 + 400 * u;
                    break;
                case Sound::Ricochet:
                    freq = 1700 * u + 600;
                    break;
                case Sound::GateYes:
                case Sound::GateYesAlt:
                    freq = 600 + 400 * (1 - u);
                    break;
                }
                v.phase += freq / 48000.f;
                if (v.phase >= 1)
                    v.phase -= std::floor(v.phase);
                float wave = std::sin(v.phase * 6.2831853f);
                if (v.sound == Sound::Zap || v.sound == Sound::Hit)
                    wave = wave * .5f + std::sin(v.phase * 31.4159f) * .5f;
                mixed += wave * u * (v.sound == Sound::Shot ? .045f : .12f);
                v.remaining -= 1 / 48000.f;
            }
        for (int channel = 0; channel < 2; channel++)
            out[i * 2 + channel] =
                audio.muted ? 0
                            : std::tanh(out[i * 2 + channel] + mixed + (channel ? right : left));
        audio.samples++;
    }
}
} // namespace tempest
