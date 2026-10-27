#include "audio.hpp"
#include "sample_data.hpp"
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
        return 10;
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
    case Sound::Snatch:
        return 25;
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
    int sample = effectSample(sound);
    int priority = sample >= 0 ? sourceSamples[size_t(sample)].priority : 1;
    switch (sound) {
    case Sound::Shot:
    case Sound::PoweredShot:
        priority = 1;
        break;
    case Sound::Kill:
    case Sound::Ricochet:
        priority = 2;
        break;
    case Sound::Zap:
    case Sound::Pulse:
        priority = 10;
        break;
    case Sound::Pickup:
        priority = 3;
        break;
    case Sound::Hit:
        priority = 50;
        break;
    case Sound::Wave:
        priority = 101;
        break;
    case Sound::Warp:
        priority = 400;
        break;
    case Sound::GateYes:
    case Sound::GateYesAlt:
        priority = 100;
        break;
    case Sound::Excellent:
        priority = 101;
        break;
    case Sound::OneUp:
        priority = 3;
        break;
    case Sound::Snatch:
        priority = 101;
        break;
    default:
        break;
    }
    auto queue = [&](int which, float period, float pan, float gain, float delay = 0.f) {
        auto *voice =
            &*std::min_element(voices.begin(), voices.end(), [](const auto &a, const auto &b) {
                if ((a.remaining <= 0) != (b.remaining <= 0))
                    return a.remaining <= 0;
                return a.priority == b.priority ? a.serial < b.serial : a.priority < b.priority;
            });
        if (voice->remaining > 0 && voice->priority > priority)
            return;
        float length = sound == Sound::Shot   ? .045f
                       : sound == Sound::Zap  ? .6f
                       : sound == Sound::Wave ? .55f
                       : sound == Sound::Warp ? 2.4f
                                              : .18f;
        *voice = {};
        voice->sound = sound;
        voice->serial = ++voiceSerial;
        voice->priority = priority;
        voice->pan = pan;
        voice->gain = gain;
        voice->delay = delay;
        if (which >= 0 && !effectSamples[size_t(which)].empty()) {
            voice->sample = which;
            voice->step = period > 0 ? float(sourceSamples[size_t(which)].period) / period : 1;
            length = float(effectSamples[size_t(which)].size()) / (48000 * 2 * voice->step);
        }
        voice->duration = voice->remaining = length;
    };
    if (sound == Sound::Excellent) {
        // sayex issues two PLAYFX2 calls with periods $160 and $162.
        queue(sample, 352, -.25f, .65f);
        queue(sample, 354, .25f, .65f);
    } else if (sound == Sound::Hit) {
        queue(sample, 200, -.25f, .65f);
        queue(sample, 196, .25f, .65f);
    } else if (sound == Sound::Wave) {
        queue(sample, 0, -.2f, .65f);
        queue(sample, 0, .2f, .65f);
    } else if (sound == Sound::Warp) {
        queue(sample, 500, -.2f, .65f);
        queue(sample, 500, .2f, .65f);
        priority = 101;
        queue(23, 512, -.2f, .65f);
        queue(23, 512, .2f, .65f);
    } else if (sound == Sound::GateYes || sound == Sound::GateYesAlt) {
        queue(sample, float(gatePeriod), -.2f, .65f);
        queue(sample, float(gatePeriod), .2f, .65f);
    } else if (sound == Sound::Kill) {
        // killme randomizes sfx_pitch over the original 128..383 period range.
        random = random * 1664525u + 1013904223u;
        queue(sample, float(128 + (random & 255)), 0, 1);
    } else if (sound == Sound::OneUp) {
        queue(sample, 512, -.25f, .8f);
        queue(sample, 514, .25f, .5f, .08f);
    } else
        queue(sample, 0, 0, 1);
    SDL_UnlockAudioDevice(device);
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
    gatePeriod = 512 + game.bonusBoosts * 5;
    for (auto &voice : voices)
        if (voice.remaining > 0 && voice.sound == Sound::Warp && voice.sample == 9)
            voice.step = float(sourceSamples[9].period) / (500 - 350 * game.transitionDepth());
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
                if (v.delay > 0) {
                    v.delay -= 1.f / 48000;
                    continue;
                }
                if (v.sample >= 0) {
                    const auto &pcm = audio.effectSamples[size_t(v.sample)];
                    size_t frame = size_t(v.position), next = frame + 1;
                    float blend = float(v.position - double(frame));
                    if (frame * 2 + 1 < pcm.size()) {
                        next = std::min(next, pcm.size() / 2 - 1);
                        float l = pcm[frame * 2] * (1 - blend) + pcm[next * 2] * blend;
                        float r = pcm[frame * 2 + 1] * (1 - blend) + pcm[next * 2 + 1] * blend;
                        left += l * .3f * v.gain * (v.pan > 0 ? 1 - v.pan : 1);
                        right += r * .3f * v.gain * (v.pan < 0 ? 1 + v.pan : 1);
                        v.position += v.step;
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
                case Sound::Snatch:
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
