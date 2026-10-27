#include "audio.hpp"
#include "domed.hpp"
#include "game.hpp"
#include "records.hpp"
#include "render.hpp"
#include "xr.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

using namespace tempest;
struct Settings {
    float sensitivity = 1;
    bool inverted = false, muted = false;
    int best = 0, unlockedLevel = 1;
    float musicVolume = .9f, sfxVolume = .32f, voiceVolume = .9f;
};
static std::string configPath() {
    char *path = SDL_GetPrefPath("de-xr", "tempest-xr");
    std::string result = path ? std::string(path) + "settings" : "";
    SDL_free(path);
    return result;
}
static std::string musicPath() {
    if (const char *overridePath = std::getenv("TEMPEST_MUSIC_DIR"))
        return overridePath;
    char *base = SDL_GetBasePath();
    std::filesystem::path installed =
        base ? std::filesystem::path(base) / "../share/tempest-xr/music" : "";
    SDL_free(base);
    if (std::filesystem::is_directory(installed))
        return installed.string();
    return TEMPEST_MUSIC_DIR;
}
static void saveCapture(const std::string &path, int width, int height) {
    std::vector<unsigned char> pixels(size_t(width) * height * 3), flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y < height; y++)
        std::copy_n(pixels.data() + size_t(height - y - 1) * width * 3, width * 3,
                    flipped.data() + size_t(y) * width * 3);
    std::ofstream output(path, std::ios::binary);
    if (!output)
        throw std::runtime_error("Cannot write screenshot: " + path);
    output << "P6\n" << width << " " << height << "\n255\n";
    output.write(reinterpret_cast<char *>(flipped.data()), std::streamsize(flipped.size()));
}
static void stereo(Renderer &renderer, const Game &game, int width, int height) {
    glClearColor(.006f, .009f, .025f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (int eye = 0; eye < 2; eye++) {
        int half = width / 2;
        glViewport(eye * half, 0, half, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float t = .05f * std::tan(63.f * 3.14159265f / 360.f),
              a = float(width) / float(std::max(1, height));
        glFrustum(-t * a, t * a, -t, t, .05, 150);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(eye == 0 ? .032f : -.032f, 0, -.55f);
        renderer.world(game, true, false);
        renderer.finishFrame(game, eye + 2, eye * half, 0, half, height);
        renderer.headsetHud(game);
    }
}
int main(int argc, char **argv) {
    bool wantXR = false, sbs = false, demo = false, smoke = false, noAudio = false, noMusic = false,
         warpDemo = false, effectsDemo = false, enemyDemo = false, explicitLevel = false;
    int bonusDemo = -1, deathDemo = -1;
    bool strictRotaryLimits = false;
    int frameLimit = 0, startingLevel = 1;
    float musicVolume = -1, sfxVolume = -1, voiceVolume = -1;
    std::string capture, musicDirectory, launchMode = "auto", reportPath;
    try {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            auto value = [&]() {
                if (i + 1 >= argc)
                    throw std::runtime_error("Missing value after " + arg);
                return std::string(argv[++i]);
            };
            if (arg == "--xr") {
                wantXR = true;
                launchMode = "xr";
            } else if (arg == "--desktop") {
                wantXR = false;
                launchMode = "desktop";
            } else if (arg == "--stereo")
                sbs = true;
            else if (arg == "--report")
                reportPath = value();
            else if (arg == "--strict-source-rotary")
                strictRotaryLimits = true;
            else if (arg == "--demo")
                demo = true;
            else if (arg == "--warp-demo")
                warpDemo = true;
            else if (arg == "--effects-demo")
                effectsDemo = true;
            else if (arg == "--enemy-demo")
                enemyDemo = true;
            else if (arg == "--death-demo")
                deathDemo = std::clamp(std::stoi(value()), 0, 1);
            else if (arg == "--bonus-demo")
                bonusDemo = std::clamp(std::stoi(value()), 0, 2);
            else if (arg == "--no-music")
                noMusic = true;
            else if (arg == "--music-dir")
                musicDirectory = value();
            else if (arg == "--music-volume")
                musicVolume = std::clamp(std::stof(value()) / 100, 0.f, 1.f);
            else if (arg == "--sfx-volume")
                sfxVolume = std::clamp(std::stof(value()) / 100, 0.f, 1.f);
            else if (arg == "--voice-volume")
                voiceVolume = std::clamp(std::stof(value()) / 100, 0.f, 1.f);
            else if (arg == "--no-audio")
                noAudio = true;
            else if (arg == "--frames")
                frameLimit = std::max(1, std::stoi(value()));
            else if (arg == "--level") {
                explicitLevel = true;
                startingLevel = std::clamp(std::stoi(value()), 1, 100);
            } else if (arg == "--capture")
                capture = value();
            else if (arg == "--smoke-test") {
                smoke = true;
                capture = value();
                frameLimit = 180;
                startingLevel = 4;
            } else if (arg == "--help") {
                std::cout
                    << "Tempest XR\nUsage: tempest-xr [--xr|--desktop] [--stereo] [--level 1..100] "
                       "[--no-audio] [--no-music] [--music-dir DIR]\n       [--demo] [--warp-demo] "
                       "[--effects-demo] [--enemy-demo] [--bonus-demo 0..2] [--frames N] "
                       "[--death-demo 0..1] [--capture image.ppm] [--report diagnostics.json] "
                       "[--smoke-test image.ppm]\n\nWheel: move | LMB: hold to fire | RMB: "
                       "Superzapper | "
                       "MMB/Space: jump\nEnter: start/retry | P/Esc: pause | "
                       "V: toggle OpenXR\nB: half-SBS preview | R: recenter XR | F11: "
                       "fullscreen\n[/]: sensitivity | I: invert wheel | M: mute | N: next music "
                       "track preview | Ctrl+Q: quit\n"
                       "Bonus: wheel/Left/Right steer, hold LMB/Up/W up, RMB/Down/S down\n"
                       "F2/F3: music volume | F4/F5: effects | F6/F7: recorded voices\n"
                       "--music-volume, --sfx-volume, --voice-volume: 0..100\n"
                       "--strict-source-rotary: literal bonus limits (some gates unreachable)\n";
                return 0;
            } else
                throw std::runtime_error("Unknown argument: " + arg);
        }
        // Tests with an explicit offscreen driver never attach to a live session.
        bool offscreen =
            getenv("SDL_VIDEODRIVER") && std::string(getenv("SDL_VIDEODRIVER")) == "offscreen";
        DomeLaunch launch = offscreen ? DomeLaunch{} : domeLaunch(launchMode);
        if (launchMode == "auto" && launch.managed)
            wantXR = launch.xr;
        DomeLink dome(launch);
        SDL_SetHint(SDL_HINT_APP_NAME, "Tempest XR");
        SDL_SetHint("SDL_APP_ID", "org.de_xr.TempestXR");
        SDL_SetHint("SDL_VIDEO_WAYLAND_WMCLASS", "org.de_xr.TempestXR");
        SDL_SetHint("SDL_VIDEO_X11_WMCLASS", "org.de_xr.TempestXR");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_TIMER |
                     SDL_INIT_GAMECONTROLLER) != 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        auto window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>(
            SDL_CreateWindow("Tempest XR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440,
                             900,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
            SDL_DestroyWindow);
        if (!window)
            throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window.get(), 960, 600);
        SDL_GLContext context = SDL_GL_CreateContext(window.get());
        if (!context)
            throw std::runtime_error(SDL_GetError());
        std::cerr << "Renderer: " << glGetString(GL_RENDERER) << " / OpenGL "
                  << glGetString(GL_VERSION) << " / " << SDL_GetCurrentVideoDriver() << '\n';
        Settings settings;
        auto path = configPath();
        if (!frameLimit) {
            std::ifstream file(path);
            // Keep the old autofire field as an ignored compatibility slot.
            bool legacyAutoFire = false;
            file >> settings.sensitivity >> settings.inverted >> settings.muted >> legacyAutoFire >>
                settings.best;
            if (!(file >> settings.unlockedLevel))
                settings.unlockedLevel = 1;
            settings.unlockedLevel = std::clamp(settings.unlockedLevel, 1, 100);
            settings.sensitivity = std::clamp(settings.sensitivity, .25f, 4.f);
            settings.best = std::max(0, settings.best);
            // Extra fields are optional so older preference files retain the new defaults.
            float m, s, v;
            if (file >> m >> s >> v) {
                settings.musicVolume = std::clamp(m, 0.f, 1.f);
                settings.sfxVolume = std::clamp(s, 0.f, 1.f);
                settings.voiceVolume = std::clamp(v, 0.f, 1.f);
            }
        }
        if (musicVolume >= 0)
            settings.musicVolume = musicVolume;
        if (sfxVolume >= 0)
            settings.sfxVolume = sfxVolume;
        if (voiceVolume >= 0)
            settings.voiceVolume = voiceVolume;
        Game game;
        game.strictRotaryLimits = strictRotaryLimits;
        game.best = std::max(settings.best, game.highScores.front().score);
        game.unlockedLevel = settings.unlockedLevel;
        game.selectedLevel = 1;
        const auto recordsPath =
            (std::filesystem::path(path).parent_path() / "records.json").string();
        if (!frameLimit)
            loadRecords(recordsPath, game);
        Renderer renderer;
        std::unique_ptr<Audio> audio;
        if (!noAudio) {
            audio = std::make_unique<Audio>();
            audio->mute(settings.muted);
            audio->setVolumes(settings.musicVolume, settings.sfxVolume, settings.voiceVolume);
            const char *effectsOverride = std::getenv("TEMPEST_EFFECTS_DIR");
            audio->setSampleDirectory(
                effectsOverride
                    ? effectsOverride
                    : (std::filesystem::path(musicPath()).parent_path() / "sfx").string());
            if (!noMusic)
                audio->setMusicDirectory(musicDirectory.empty() ? musicPath() : musicDirectory);
        }
        std::unique_ptr<XR> xr;
        std::string status = "DESKTOP / WHEEL CONTROL / 37 ORIGINAL WEB SHAPES";
        bool xrIntent = false, captured = false, everFocused = false;
        int xrEntries = 0, xrSuspends = 0, pausedSuspends = 0, wheelEvents = 0;
        auto captureInput = [&](bool enabled) {
            SDL_SetWindowMouseGrab(window.get(), enabled ? SDL_TRUE : SDL_FALSE);
            SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
            SDL_ShowCursor(enabled ? SDL_DISABLE : SDL_ENABLE);
            captured = enabled;
        };
        auto suspendXR = [&]() {
            if (game.active())
                game.pause();
            ++xrSuspends;
            if (game.mode == Mode::Paused)
                ++pausedSuspends;
            game.fireHeld = false;
            game.bonusUp = game.bonusDown = false;
            if (xr)
                xr->setVisible(false);
            captureInput(false);
            xr.reset();
            dome.suspend();
            SDL_GL_SetSwapInterval(1);
            status = "XR PAUSED / SELECT TEMPEST XR TO RETURN / CLICK OR ENTER TO RESUME";
        };
        // Every terminal XR path must release both SDL capture and the compositor lease.
        auto leaveXR = [&](bool pause) {
            xrIntent = false;
            everFocused = false;
            game.fireHeld = false;
            game.bonusUp = game.bonusDown = false;
            if (pause && game.active())
                game.pause();
            captureInput(false);
            dome.end(true);
            xr.reset();
            SDL_GL_SetSwapInterval(1);
        };
        auto enterXR = [&]() {
            try {
                dome.ready();
                auto candidate = std::make_unique<XR>();
                candidate->initialize(window.get());
                dome.begin();
                xr = std::move(candidate);
                xrIntent = true;
                everFocused = false;
                ++xrEntries;
                captureInput(true);
                SDL_GL_SetSwapInterval(0);
                status = "OPENXR / " + xr->description() + " / R TO RECENTER";
            } catch (const std::exception &e) {
                leaveXR(true);
                std::cerr << "XR unavailable: " << e.what() << '\n';
                status = std::string("XR UNAVAILABLE - DESKTOP ACTIVE / ") + e.what();
                if (status.size() > 110)
                    status.resize(110);
            }
        };
        auto toggleXR = [&]() {
            if (xrIntent) {
                leaveXR(false);
                SDL_RaiseWindow(window.get());
                status = "DESKTOP / XR SESSION CLOSED";
            } else
                enterXR();
        };
        SDL_GL_SetSwapInterval(frameLimit ? 0 : 1);
        if (wantXR)
            toggleXR();
        if (demo || smoke || warpDemo) {
            game.start(startingLevel);
            game.fireHeld = demo || smoke;
        }
        if (enemyDemo) {
            game.start(explicitLevel ? startingLevel : 33);
            game.enemiesRemaining = 0;
            game.fireHeld = false;
            for (int type = 0; type < 12; type++)
                game.spawn(Game::kindForSource(type), type % game.web.lanes(),
                           .65f + float(type % 3) * .1f);
        }
        if (deathDemo >= 0) {
            game.start(startingLevel);
            game.fireHeld = false;
            game.invulnerable = 0;
            game.spawn(deathDemo ? Kind::Flipper : Kind::EnemyShot, game.lane, .0001f);
        }
        if (bonusDemo >= 0) {
            game.start(startingLevel);
            game.startBonus(bonusDemo);
        }
        if (warpDemo) {
            game.jumpEnabled = true;
            game.enemiesRemaining = 0;
            game.tick(1.f / 120);
        }
        bool quit = false, fullscreen = false;
        bool persist = !frameLimit && !demo && !enemyDemo && !effectsDemo && bonusDemo < 0 &&
                       !warpDemo && !explicitLevel && deathDemo < 0;
        int savedUnlocked = game.unlockedLevel, savedRecords = game.recordsVersion;
        Mode lastMode = game.mode;
        auto saveSettings = [&]() {
            if (!persist)
                return;
            std::string temporary = path + ".tmp";
            std::ofstream file(temporary);
            if (!file) {
                std::cerr << "Cannot save preferences: " << path << '\n';
                return;
            }
            file << settings.sensitivity << ' ' << settings.inverted << ' ' << settings.muted << ' '
                 << 0 << ' ' << game.best << ' ' << game.unlockedLevel << ' '
                 << settings.musicVolume << ' ' << settings.sfxVolume << ' ' << settings.voiceVolume
                 << '\n';
            file.close();
            std::error_code error;
            std::filesystem::rename(temporary, path, error);
            if (error)
                std::cerr << "Cannot save preferences: " << error.message() << '\n';
            if (!saveRecords(recordsPath, game))
                std::cerr << "Cannot save score table and access keys: " << recordsPath << '\n';
            savedUnlocked = game.unlockedLevel;
            savedRecords = game.recordsVersion;
        };
        std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)> secondary(
            nullptr, SDL_GameControllerClose);
        auto connectSecondary = [&]() {
            secondary.reset();
            int pads = 0;
            for (int i = 0; i < SDL_NumJoysticks(); ++i)
                if (SDL_IsGameController(i) && ++pads == 2) {
                    secondary.reset(SDL_GameControllerOpen(i));
                    break;
                }
        };
        connectSecondary();
        int frame = 0, smokeBefore = -1, smokeAfter = -1;
        float keyboardTimer = 0;
        uint64_t previous = SDL_GetPerformanceCounter();
        double accumulator = 0;
        while (!quit) {
            uint64_t now = SDL_GetPerformanceCounter();
            double elapsed = double(now - previous) / double(SDL_GetPerformanceFrequency());
            previous = now;
            if (smoke && frame == 20) {
                smokeBefore = game.lane;
                SDL_Event event{};
                event.type = SDL_MOUSEWHEEL;
                event.wheel.y = 2;
                event.wheel.preciseY = 2;
                SDL_PushEvent(&event);
            }
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT)
                    quit = true;
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    game.fireHeld = false;
                    game.bonusUp = game.bonusDown = false;
                    // DomeD's lease is authoritative; transient queued SDL focus
                    // changes during capture must not start a suspend/resume loop.
                    if (xr && captured && !dome.managed())
                        suspendXR();
                    else if (!xr && game.active() && !frameLimit)
                        game.pause();
                }
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && xrIntent && !captured) {
                    if (dome.focused())
                        enterXR();
                }
                if (event.type == SDL_CONTROLLERDEVICEADDED ||
                    event.type == SDL_CONTROLLERDEVICEREMOVED)
                    connectSecondary();
                if (event.type == SDL_MOUSEWHEEL) {
                    ++wheelEvents;
                    float delta = event.wheel.preciseY;
                    if (delta == 0)
                        delta = float(event.wheel.y);
                    if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                        delta = -delta;
                    game.rotary(delta * settings.sensitivity * (settings.inverted ? -1.f : 1.f));
                }
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (game.enteringScore) {
                            game.nextScoreLetter();
                            continue;
                        }
                        if (game.mode == Mode::Paused)
                            game.pause();
                        if (game.isBonus()) {
                            game.bonusUp = true;
                            continue;
                        }
                        game.fireHeld = true;
                        if (game.mode == Mode::Menu || game.mode == Mode::GameOver ||
                            game.mode == Mode::Victory)
                            game.activate();
                    }
                    if (event.button.button == SDL_BUTTON_RIGHT) {
                        if (game.enteringScore) {
                            game.scoreCursor = std::max(0, game.scoreCursor - 1);
                            continue;
                        }
                        if (game.isBonus()) {
                            game.bonusDown = true;
                            continue;
                        }
                        game.zap();
                    }
                    if (event.button.button == SDL_BUTTON_MIDDLE)
                        game.jump();
                }
                if (event.type == SDL_MOUSEBUTTONUP) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        game.fireHeld = false;
                        game.bonusUp = false;
                    }
                    if (event.button.button == SDL_BUTTON_RIGHT)
                        game.bonusDown = false;
                }
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    auto key = event.key.keysym.sym;
                    if (game.enteringScore && !(event.key.keysym.mod & (KMOD_CTRL | KMOD_ALT))) {
                        if (key == SDLK_RETURN)
                            game.finishScoreEntry();
                        else if (key == SDLK_ESCAPE) {
                            game.enteringScore = false;
                            game.returnToTitle();
                        } else if (key == SDLK_BACKSPACE || key == SDLK_LEFT)
                            game.scoreCursor = std::max(0, game.scoreCursor - 1);
                        else if (key == SDLK_RIGHT)
                            game.nextScoreLetter();
                        else if (key == SDLK_UP)
                            game.steer(1);
                        else if (key == SDLK_DOWN)
                            game.steer(-1);
                        else if (key >= SDLK_a && key <= SDLK_z)
                            game.setScoreLetter(char('A' + key - SDLK_a));
                        else if (key == SDLK_SPACE)
                            game.setScoreLetter(' ');
                        else if (key == SDLK_PERIOD)
                            game.setScoreLetter('.');
                        else if (key == SDLK_MINUS)
                            game.setScoreLetter('-');
                        else if (key == SDLK_EXCLAIM ||
                                 (key == SDLK_1 && (event.key.keysym.mod & KMOD_SHIFT)))
                            game.setScoreLetter('!');
                        continue;
                    }
                    if (key == SDLK_k && game.mode == Mode::Menu)
                        game.selectAccessKey();
                    if (key == SDLK_o && game.mode == Mode::Menu && game.beastlyUnlocked)
                        game.beastly = !game.beastly;
                    if (key == SDLK_q && (event.key.keysym.mod & KMOD_CTRL))
                        quit = true;
                    if (key == SDLK_TAB && (event.key.keysym.mod & KMOD_ALT) && xr) {
                        suspendXR();
                        continue;
                    }
                    if (key == SDLK_RETURN && game.mode == Mode::Paused)
                        game.pause();
                    if (key == SDLK_RETURN &&
                        (game.mode == Mode::Menu || game.mode == Mode::GameOver ||
                         game.mode == Mode::Victory))
                        game.activate();
                    if (key == SDLK_ESCAPE) {
                        if (game.mode == Mode::Menu) {
                            if (game.frontPage == FrontPage::Keys)
                                game.returnToTitle();
                            else
                                quit = true;
                        } else if (game.mode == Mode::GameOver || game.mode == Mode::Victory)
                            game.returnToTitle();
                        else
                            game.pause();
                    }
                    if (key == SDLK_p)
                        game.pause();
                    if (key == SDLK_SPACE)
                        game.jump();
                    if (key == SDLK_z)
                        game.zap();
                    if (key == SDLK_i)
                        settings.inverted = !settings.inverted;
                    if (key == SDLK_LEFTBRACKET)
                        settings.sensitivity = std::max(.25f, settings.sensitivity - .25f);
                    if (key == SDLK_RIGHTBRACKET)
                        settings.sensitivity = std::min(4.f, settings.sensitivity + .25f);
                    if (key == SDLK_m) {
                        settings.muted = !settings.muted;
                        if (audio)
                            audio->mute(settings.muted);
                    }
                    if (key == SDLK_n && audio)
                        audio->nextTrack();
                    if (key >= SDLK_F2 && key <= SDLK_F7) {
                        float &volume = key <= SDLK_F3   ? settings.musicVolume
                                        : key <= SDLK_F5 ? settings.sfxVolume
                                                         : settings.voiceVolume;
                        bool up = key == SDLK_F3 || key == SDLK_F5 || key == SDLK_F7;
                        volume = std::clamp(volume + (up ? .05f : -.05f), 0.f, 1.f);
                        if (audio)
                            audio->setVolumes(settings.musicVolume, settings.sfxVolume,
                                              settings.voiceVolume);
                    }
                    if (key == SDLK_v)
                        toggleXR();
                    if (key == SDLK_r && xr)
                        xr->recenter();
                    if (key == SDLK_b)
                        sbs = !sbs;
                    if (key == SDLK_F11) {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window.get(),
                                                fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    }
                }
            }
            if (smoke && frame == 20)
                smokeAfter = game.lane;
            if (secondary && !SDL_GameControllerGetAttached(secondary.get()))
                connectSecondary();
            if (secondary) {
                auto down = [&](SDL_GameControllerButton b) {
                    return SDL_GameControllerGetButton(secondary.get(), b) != 0;
                };
                game.secondaryPad((down(SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? 1u : 0u) |
                                      (down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ? 2u : 0u),
                                  (down(SDL_CONTROLLER_BUTTON_DPAD_UP) ? 1u : 0u) |
                                      (down(SDL_CONTROLLER_BUTTON_DPAD_DOWN) ? 2u : 0u));
            } else
                game.secondaryPad(0, 0, false);
            auto keys = SDL_GetKeyboardState(nullptr);
            if (game.isBonus() && game.mode != Mode::Paused) {
                auto buttons = SDL_GetMouseState(nullptr, nullptr);
                game.bonusUp =
                    (buttons & SDL_BUTTON_LMASK) || keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
                game.bonusDown =
                    (buttons & SDL_BUTTON_RMASK) || keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
            }
            keyboardTimer -= float(elapsed);
            if (keyboardTimer <= 0 && !game.enteringScore) {
                if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
                    game.steer(-1);
                    keyboardTimer = .085f;
                } else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
                    game.steer(1);
                    keyboardTimer = .085f;
                }
            }
            if (demo && game.mode == Mode::Playing) {
                game.fireHeld = true; // Attract/demo input, matching the source's auto branch.
                const Enemy *target = nullptr;
                for (auto &e : game.enemies)
                    if (e.kind != Kind::Powerup && (!target || e.depth < target->depth))
                        target = &e;
                if (target)
                    game.steer(float(target->lane - game.lane));
            }
            if (smoke && frame == 80)
                game.jumpEnabled = true;
            if (smoke && frame == 80)
                game.jump();
            if (smoke && frame == 135)
                game.zap();
            if (effectsDemo) {
                if (frame == 5) {
                    game.start(startingLevel);
                    game.fireHeld = true; // Scripted firing for the effects fixture.
                }
                if (frame == 20)
                    game.steer(3);
                if (frame == 40)
                    game.jumpEnabled = true;
                if (frame == 40)
                    game.jump();
                if (frame == 90) {
                    game.boost = true;
                    game.burst(game.shipPosition(), 3, 100);
                }
                if (frame == 130)
                    game.zap();
                if (frame == 160) {
                    game.enemies.clear();
                    game.enemiesRemaining = 0;
                }
                if (frame == 270)
                    game.jump();
            }
            if (xr && captured && dome.suspended())
                suspendXR();
            if (xr)
                xr->setVisible(captured && dome.active());
            accumulator += frameLimit ? 1. / 60. : std::min(elapsed, .1);
            while (accumulator >= 1. / 120.) {
                if (!xr || (captured && dome.active() && xr->focused()))
                    game.tick(1.f / 120);
                accumulator -= 1. / 120.;
            }
            if (persist &&
                (game.recordsVersion != savedRecords || game.unlockedLevel > savedUnlocked ||
                 (game.mode != lastMode &&
                  (game.mode == Mode::GameOver || game.mode == Mode::Victory))))
                saveSettings();
            lastMode = game.mode;
            if (audio) {
                audio->updateMusic(
                    game, xrIntent && (!captured || !dome.active() || (xr && !xr->focused())));
                for (auto sound : game.sounds)
                    audio->play(sound);
            }
            game.sounds.clear();
            if (xr) {
                try {
                    if (!xr->frame(renderer, game)) {
                        leaveXR(true);
                        status = "XR SESSION ENDED / DESKTOP ACTIVE";
                        SDL_GL_SetSwapInterval(1);
                    } else if (xr->focused())
                        everFocused = true;
                    else if (everFocused && captured)
                        suspendXR();
                } catch (const std::exception &e) {
                    std::cerr << "XR session ended: " << e.what() << '\n';
                    leaveXR(true);
                    status = "XR SESSION LOST / DESKTOP ACTIVE";
                    SDL_GL_SetSwapInterval(1);
                }
            }
            int width, height;
            SDL_GL_GetDrawableSize(window.get(), &width, &height);
            if (width > 0 && height > 0) {
                if (sbs)
                    stereo(renderer, game, width, height);
                else {
                    renderer.desktop(game, width, height);
                    std::string audioStatus = status;
                    if (audio && (game.mode == Mode::Paused || game.mode == Mode::Menu)) {
                        audioStatus =
                            audio->musicStatus() + " / F2-3 MUSIC " +
                            std::to_string(int(settings.musicVolume * 100 + .5f)) + " / F4-5 SFX " +
                            std::to_string(int(settings.sfxVolume * 100 + .5f)) + " / F6-7 VOICE " +
                            std::to_string(int(settings.voiceVolume * 100 + .5f));
                    }
                    renderer.finishFrame(game, 4, 0, 0, width, height);
                    renderer.hud(game, width, height, audioStatus, settings.sensitivity,
                                 settings.inverted, settings.muted);
                }
                if (frameLimit && frame + 1 >= frameLimit && !capture.empty())
                    saveCapture(capture, width, height);
                SDL_GL_SwapWindow(window.get());
            }
            frame++;
            if (frameLimit && frame >= frameLimit)
                quit = true;
            if (!frameLimit && !xr)
                SDL_Delay(1);
        }
        GLenum error = glGetError();
        if (frameLimit && error != GL_NO_ERROR)
            throw std::runtime_error("Rendering validation failed: OpenGL error " +
                                     std::to_string(error));
        if (smoke) {
            bool wheel = smokeAfter == game.boundedLane(smokeBefore + 3);
            std::cout << "{\"frames\":" << frame << ",\"wheel_before\":" << smokeBefore
                      << ",\"wheel_after\":" << smokeAfter
                      << ",\"wheel_ok\":" << (wheel ? "true" : "false")
                      << ",\"score\":" << game.score << ",\"gl_error\":" << error << "}\n";
            if (!wheel || error != GL_NO_ERROR)
                throw std::runtime_error("Smoke validation failed");
        }
        saveSettings();
        if (!reportPath.empty())
            std::ofstream(reportPath)
                << nlohmann::json{{"score", game.score},
                                  {"beastly", game.beastly},
                                  {"access_key", game.accessKey},
                                  {"player_shots", game.shotSlots(false)},
                                  {"managed", dome.managed()},
                                  {"launch_xr", launch.xr},
                                  {"xr_entries", xrEntries},
                                  {"xr_suspends", xrSuspends},
                                  {"paused_suspends", pausedSuspends},
                                  {"wheel_events", wheelEvents},
                                  {"paused", game.mode == Mode::Paused},
                                  {"captured", captured},
                                  {"xr_intent", xrIntent},
                                  {"relative_mouse", SDL_GetRelativeMouseMode() == SDL_TRUE},
                                  {"gl_error", error}}
                       .dump(2)
                << '\n';
        captureInput(false);
        dome.end();
        xr.reset();
        audio.reset();
        renderer.releaseGL();
        SDL_GL_DeleteContext(context);
        window.reset();
        secondary.reset();
        SDL_Quit();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "tempest-xr: " << e.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
