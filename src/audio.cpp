#include "audio.hpp"
#include "sample_data.hpp"
#include <cctype>
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
                loopStarts[size_t(i)] = sourceSamples[size_t(i)].loopStart * 48000.0 / spec.freq;
                loopEnds[size_t(i)] = std::min(double(count / 2), sourceSamples[size_t(i)].loopEnd *
                                                                      48000.0 / spec.freq);
                loaded++;
            }
        }
        SDL_FreeWAV(bytes);
    }
    SDL_UnlockAudioDevice(device);
    std::cerr << "Original effects: " << loaded << " samples\n";
}
void Audio::setVolumes(float music, float effects, float voice) {
    if (!device)
        return;
    SDL_LockAudioDevice(device);
    musicVolume = std::clamp(music, 0.f, 1.f);
    effectsVolume = std::clamp(effects, 0.f, 1.f);
    voiceVolume = std::clamp(voice, 0.f, 1.f);
    SDL_UnlockAudioDevice(device);
}
std::string Audio::musicStatus() const {
    std::string track = currentTrack;
    if (track.size() > 4)
        track.resize(track.size() - 4);
    std::transform(track.begin(), track.end(), track.begin(),
                   [](unsigned char ch) { return char(std::toupper(ch)); });
    return "MUSIC " + (track.empty() ? "OFF" : track) +
           (manuallySelected ? " / PREVIEW UNTIL NEXT WEB" : " / AUTO");
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
        return 9;
    case Sound::Warp:
        return 9;
    case Sound::PoweredShot:
        return 11;
    case Sound::Excellent:
    case Sound::BonusToken:
        return 21;
    case Sound::OneUp:
        return 24;
    case Sound::Pulse:
        return 7;
    case Sound::Ricochet:
        return 20;
    case Sound::GateYes:
        return 26;
    case Sound::GateYesAlt:
        return 27;
    case Sound::Snatch:
        return 25;
    case Sound::Yes:
        return 23;
    case Sound::ClearanceEngine:
        return 0;
    case Sound::FlipperLand:
        return 28;
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
    case Sound::FlipperLand:
        priority = 2;
        break;
    case Sound::Zap:
        priority = 10;
        break;
    case Sound::Pulse:
        priority = 51;
        break;
    case Sound::Pickup:
        priority = 3;
        break;
    case Sound::Hit:
        priority = 50;
        break;
    case Sound::Wave:
    case Sound::Yes:
        priority = 101;
        break;
    case Sound::Warp:
        priority = 400;
        break;
    case Sound::GateYes:
    case Sound::GateYesAlt:
    case Sound::ClearanceEngine:
        priority = 100;
        break;
    case Sound::Excellent:
        priority = 101;
        break;
    case Sound::BonusToken:
        priority = 90;
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
    auto queue = [&](int which, int pitch, float pan, float gain, bool loop = false) {
        // Missing samples are silent: speech and effects both come from the source PCM.
        if (which < 0 || effectSamples[size_t(which)].empty())
            return;
        auto *voice = &*std::min_element(
            voices.begin(), voices.begin() + effectChannels, [](const auto &a, const auto &b) {
                if ((a.remaining <= 0) != (b.remaining <= 0))
                    return a.remaining <= 0;
                return a.priority == b.priority ? a.serial < b.serial : a.priority < b.priority;
            });
        if (voice->remaining > 0 && voice->priority > priority)
            return;
        *voice = {};
        voice->sound = sound;
        voice->serial = ++voiceSerial;
        voice->priority = priority;
        voice->pan = pan;
        voice->gain = gain;
        voice->loop = loop;
        voice->loopStart = loopStarts[size_t(which)];
        voice->loopEnd = loopEnds[size_t(which)];
        if (voice->loopEnd <= voice->loopStart)
            voice->loop = false;
        float length = 0;
        if (which >= 0 && !effectSamples[size_t(which)].empty()) {
            voice->sample = which;
            voice->step = pitch > 0 ? float(effectStep(pitch)) /
                                          effectStep(sourceSamples[size_t(which)].pitch)
                                    : 1;
            length = float(effectSamples[size_t(which)].size()) / (48000 * 2 * voice->step);
        }
        voice->remaining = length;
    };
    if (sound == Sound::Excellent) {
        // sayex issues two PLAYFX2 calls with pitch words $160 and $162.
        queue(sample, 352, -.25f, .65f);
        queue(sample, 354, .25f, .65f);
    } else if (sound == Sound::Hit) {
        queue(sample, 200, -.25f, .65f);
        queue(sample, 196, .25f, .65f);
    } else if (sound == Sound::Snatch || sound == Sound::BonusToken) {
        queue(sample, 0, -.2f, .65f);
        queue(sample, 0, .2f, .65f);
    } else if (sound == Sound::Pulse) {
        queue(sample, 500, 0, 1);
    } else if (sound == Sound::Wave) {
        queue(sample, 0, -.2f, .65f);
        queue(sample, 0, .2f, .65f);
    } else if (sound == Sound::Warp) {
        queue(sample, 0, -.2f, .65f);
        queue(sample, 0, .2f, .65f);
    } else if (sound == Sound::Yes) {
        queue(23, 512, -.2f, .65f, true);
        queue(23, 512, .2f, .65f, true);
    } else if (sound == Sound::ClearanceEngine) {
        queue(0, 10, -.2f, .65f, true);
        queue(2, 20, .2f, .65f, true);
    } else if (sound == Sound::Pickup) {
        queue(sample, 428, 0, 1);
    } else if (sound == Sound::GateYes || sound == Sound::GateYesAlt) {
        queue(sample, gatePeriod, -.2f, .65f);
        queue(sample, gatePeriod, .2f, .65f);
    } else if (sound == Sound::Kill) {
        // killme randomizes sfx_pitch over the original 128..383 pitch range.
        random = random * 1664525u + 1013904223u;
        queue(sample, int(128 + (random & 255)), 0, 1);
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
    bool title =
        game.mode == Mode::Menu || game.mode == Mode::GameOver || game.mode == Mode::Victory;
    int context = title ? -2 - int(game.mode) : game.isBonus() ? 1000 + game.bonusType : game.level;
    if (context != musicContext) {
        manuallySelected = false;
        musicContext = context;
    }
    static const char *allTracks[] = {"tune13.mod", "tune7.mod", "tune1.mod", "tune3.mod",
                                      "rave4.mod",  "tune5.mod", "tune12.mod"};
    std::string desired = manuallySelected ? allTracks[manualTrack % 7]
                          : game.isBonus()
                              ? (game.bonusType == 1 ? "tune3.mod" : "tune1.mod")
                              : Music::trackForLevel(game.level, game.mode == Mode::Menu ||
                                                                     game.mode == Mode::GameOver ||
                                                                     game.mode == Mode::Victory);
    if (!musicDirectory.empty() && desired != currentTrack &&
        (failedTrack != desired || SDL_GetTicks64() >= retryMusicAt)) {
        SDL_LockAudioDevice(device);
        bool loaded = music.load((std::filesystem::path(musicDirectory) / desired).string());
        SDL_UnlockAudioDevice(device);
        if (loaded) {
            currentTrack = desired;
            failedTrack.clear();
        } else {
            failedTrack = desired;
            retryMusicAt = SDL_GetTicks64() + 2000;
        }
        std::cerr << (loaded ? "Soundtrack: " : "Soundtrack unavailable: ") << desired << '\n';
    }
    SDL_LockAudioDevice(device);
    if (game.mode != Mode::Paused && game.mode != audioMode) {
        for (auto &voice : voices)
            if (voice.sound == Sound::ClearanceEngine || voice.sound == Sound::Yes)
                voice.remaining = 0;
        audioMode = game.mode;
    }
    if (game.deathTime > 0)
        for (auto &voice : voices)
            if (voice.loop)
                voice.remaining = 0;
    size_t channels = currentTrack.empty() ? 6 : 2;
    if (channels < effectChannels)
        for (size_t i = channels; i < voices.size(); ++i)
            voices[i].remaining = 0;
    effectChannels = channels;
    gatePeriod = 512 + game.bonusBoosts * 5;
    for (auto &voice : voices)
        if (voice.remaining > 0 && voice.sound == Sound::ClearanceEngine) {
            int pitch = int(10 + game.transitionDepth() * 144);
            if (voice.sample == 2)
                pitch *= 2;
            voice.step =
                float(effectStep(pitch)) / effectStep(sourceSamples[size_t(voice.sample)].pitch);
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
        float left = 0, right = 0, voiceLeft = 0, voiceRight = 0;
        bool originalBusy = false;
        for (const auto &v : audio.voices)
            if (v.remaining > 0 && (v.sound == Sound::Wave || v.sound == Sound::Excellent ||
                                    v.sound == Sound::Yes || v.sound == Sound::Snatch))
                originalBusy = true;
        bool narration = originalBusy;
        float targetDuck = narration ? 1.f : 0.f;
        audio.duck += (targetDuck - audio.duck) / (48000 * (narration ? .02f : .25f));
        for (auto &v : audio.voices)
            if (v.remaining > 0) {
                if (v.sample >= 0) {
                    if (v.loop && v.position >= v.loopEnd)
                        v.position = v.loopStart +
                                     std::fmod(v.position - v.loopStart, v.loopEnd - v.loopStart);
                    const auto &pcm = audio.effectSamples[size_t(v.sample)];
                    size_t frame = size_t(v.position), next = frame + 1;
                    float blend = float(v.position - double(frame));
                    if (frame * 2 + 1 < pcm.size()) {
                        next = std::min(next, pcm.size() / 2 - 1);
                        float l = pcm[frame * 2] * (1 - blend) + pcm[next * 2] * blend;
                        float r = pcm[frame * 2 + 1] * (1 - blend) + pcm[next * 2 + 1] * blend;
                        float pl = l * .3f * v.gain * (v.pan > 0 ? 1 - v.pan : 1),
                              pr = r * .3f * v.gain * (v.pan < 0 ? 1 + v.pan : 1);
                        if (v.sample >= 21 && v.sample <= 27) {
                            voiceLeft += pl;
                            voiceRight += pr;
                        } else {
                            left += pl;
                            right += pr;
                        }
                        v.position += v.step;
                    } else
                        v.remaining = 0;
                    if (!v.loop)
                        v.remaining -= 1.f / 48000;
                    continue;
                }
            }
        for (int channel = 0; channel < 2; channel++) {
            float music = out[i * 2 + channel] * audio.musicVolume * (1 - .22f * audio.duck);
            float effects = .4f * std::tanh((channel ? right : left) / .4f) * audio.effectsVolume *
                            (1 - .6f * audio.duck);
            float recorded = channel ? voiceRight : voiceLeft;
            out[i * 2 + channel] =
                audio.muted ? 0 : std::tanh(music + effects + recorded * audio.voiceVolume);
        }
        audio.samples++;
    }
}
} // namespace tempest
