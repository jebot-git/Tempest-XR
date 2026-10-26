#include "audio.hpp"
#include "game.hpp"
#include "render.hpp"
#include "xr.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace tempest;
struct Settings {
    float sensitivity = 1;
    bool inverted = false, muted = false, autoFire = true;
    int best = 0, unlockedLevel = 1;
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
        renderer.world(game, true);
        renderer.finishFrame(game, eye + 2, eye * half, 0, half, height);
    }
}
int main(int argc, char **argv) {
    bool wantXR = false, sbs = false, demo = false, smoke = false, noAudio = false, noMusic = false,
         warpDemo = false, effectsDemo = false, enemyDemo = false, explicitLevel = false;
    int bonusDemo = -1;
    int frameLimit = 0, startingLevel = 1;
    std::string capture, musicDirectory;
    try {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            auto value = [&]() {
                if (i + 1 >= argc)
                    throw std::runtime_error("Missing value after " + arg);
                return std::string(argv[++i]);
            };
            if (arg == "--xr")
                wantXR = true;
            else if (arg == "--stereo")
                sbs = true;
            else if (arg == "--demo")
                demo = true;
            else if (arg == "--warp-demo")
                warpDemo = true;
            else if (arg == "--effects-demo")
                effectsDemo = true;
            else if (arg == "--enemy-demo")
                enemyDemo = true;
            else if (arg == "--bonus-demo")
                bonusDemo = std::clamp(std::stoi(value()), 0, 2);
            else if (arg == "--no-music")
                noMusic = true;
            else if (arg == "--music-dir")
                musicDirectory = value();
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
                    << "Tempest XR prototype\nUsage: tempest-xr [--xr] [--stereo] [--level 1..100] "
                       "[--no-audio] [--no-music] [--music-dir DIR]\n       [--demo] [--warp-demo] "
                       "[--effects-demo] [--enemy-demo] [--bonus-demo 0..2] [--frames N] "
                       "[--capture image.ppm] "
                       "[--smoke-test image.ppm]\n\nWheel: move | LMB: fire | RMB: Superzapper | "
                       "MMB/Space: jump\nEnter: start/retry | P/Esc: pause | F: automatic fire | "
                       "V: toggle OpenXR\nB: half-SBS preview | R: recenter XR | F11: "
                       "fullscreen\n[/]: sensitivity | I: invert wheel | M: mute | N: next music "
                       "track | Ctrl+Q: quit\n";
                return 0;
            } else
                throw std::runtime_error("Unknown argument: " + arg);
        }
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        auto window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>(
            SDL_CreateWindow("TEMPEST 2000 / XR - source study", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 1440, 900,
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
            file >> settings.sensitivity >> settings.inverted >> settings.muted >>
                settings.autoFire >> settings.best;
            if (!(file >> settings.unlockedLevel))
                settings.unlockedLevel = 1;
            settings.unlockedLevel = std::clamp(settings.unlockedLevel, 1, 100);
            settings.sensitivity = std::clamp(settings.sensitivity, .25f, 4.f);
            settings.best = std::max(0, settings.best);
        }
        Game game;
        game.best = settings.best;
        game.autoFire = settings.autoFire;
        game.unlockedLevel = settings.unlockedLevel;
        game.selectedLevel = explicitLevel ? startingLevel : settings.unlockedLevel;
        Renderer renderer;
        std::unique_ptr<Audio> audio;
        if (!noAudio) {
            audio = std::make_unique<Audio>();
            audio->mute(settings.muted);
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
        auto toggleXR = [&]() {
            if (xr) {
                xr.reset();
                SDL_GL_SetSwapInterval(1);
                status = "DESKTOP / XR SESSION CLOSED";
                return;
            }
            try {
                auto candidate = std::make_unique<XR>();
                candidate->initialize(window.get());
                xr = std::move(candidate);
                SDL_GL_SetSwapInterval(0);
                status = "OPENXR / " + xr->description() + " / R TO RECENTER";
            } catch (const std::exception &e) {
                std::cerr << "XR unavailable: " << e.what() << '\n';
                status = std::string("XR UNAVAILABLE - DESKTOP ACTIVE / ") + e.what();
                if (status.size() > 110)
                    status.resize(110);
            }
        };
        SDL_GL_SetSwapInterval(frameLimit ? 0 : 1);
        if (wantXR)
            toggleXR();
        if (demo || smoke || warpDemo)
            game.start(startingLevel);
        if (enemyDemo) {
            game.start(explicitLevel ? startingLevel : 33);
            game.enemiesRemaining = 0;
            game.autoFire = false;
            for (int type = 0; type < 12; type++)
                game.spawn(Game::kindForSource(type), type % game.web.lanes(),
                           .65f + float(type % 3) * .1f);
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
                       !warpDemo && !explicitLevel;
        int savedUnlocked = game.unlockedLevel;
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
                 << game.autoFire << ' ' << game.best << ' ' << game.unlockedLevel << '\n';
            file.close();
            std::error_code error;
            std::filesystem::rename(temporary, path, error);
            if (error)
                std::cerr << "Cannot save preferences: " << error.message() << '\n';
            savedUnlocked = game.unlockedLevel;
        };
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
                    if (!xr &&
                        (game.mode == Mode::Playing || game.mode == Mode::Transition ||
                         game.mode == Mode::Bonus) &&
                        !frameLimit)
                        game.pause();
                }
                if (event.type == SDL_MOUSEWHEEL) {
                    float delta = event.wheel.preciseY;
                    if (delta == 0)
                        delta = float(event.wheel.y);
                    if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                        delta = -delta;
                    game.steer(delta * settings.sensitivity * (settings.inverted ? -1.f : 1.f));
                }
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (game.isBonus()) {
                            game.bonusUp = true;
                            continue;
                        }
                        game.fireHeld = true;
                        if (((xr || sbs) && game.mode == Mode::Menu) ||
                            game.mode == Mode::GameOver || game.mode == Mode::Victory)
                            game.start(game.mode == Mode::Victory ? 1
                                       : explicitLevel            ? startingLevel
                                                                  : game.selectedLevel);
                        int w, h;
                        SDL_GetWindowSize(window.get(), &w, &h);
                        float x = float(event.button.x) * 1440 / w,
                              y = float(event.button.y) * 900 / h;
                        if (game.mode == Mode::Menu && x >= 70 && x <= 480 && y >= 662 && y <= 724)
                            game.start(explicitLevel ? startingLevel : game.selectedLevel);
                    }
                    if (event.button.button == SDL_BUTTON_RIGHT) {
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
                    if (key == SDLK_q && (event.key.keysym.mod & KMOD_CTRL))
                        quit = true;
                    if (key == SDLK_RETURN &&
                        (game.mode == Mode::Menu || game.mode == Mode::GameOver ||
                         game.mode == Mode::Victory))
                        game.start(game.mode == Mode::Victory ? 1
                                   : explicitLevel            ? startingLevel
                                                              : game.selectedLevel);
                    if (key == SDLK_ESCAPE) {
                        if (game.mode == Mode::Menu)
                            quit = true;
                        else if (game.mode == Mode::GameOver || game.mode == Mode::Victory)
                            game.mode = Mode::Menu;
                        else
                            game.pause();
                    }
                    if (key == SDLK_p)
                        game.pause();
                    if (key == SDLK_SPACE)
                        game.jump();
                    if (key == SDLK_z)
                        game.zap();
                    if (key == SDLK_f)
                        game.autoFire = !game.autoFire;
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
            auto keys = SDL_GetKeyboardState(nullptr);
            keyboardTimer -= float(elapsed);
            if (keyboardTimer <= 0) {
                if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
                    game.steer(-1);
                    keyboardTimer = .085f;
                } else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
                    game.steer(1);
                    keyboardTimer = .085f;
                }
            }
            if (demo && game.mode == Mode::Playing) {
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
                if (frame == 5)
                    game.start(startingLevel);
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
            accumulator += frameLimit ? 1. / 60. : std::min(elapsed, .1);
            while (accumulator >= 1. / 120.) {
                if (!xr || xr->focused())
                    game.tick(1.f / 120);
                accumulator -= 1. / 120.;
            }
            if (persist && (game.unlockedLevel > savedUnlocked ||
                            (game.mode != lastMode &&
                             (game.mode == Mode::GameOver || game.mode == Mode::Victory))))
                saveSettings();
            lastMode = game.mode;
            if (audio) {
                audio->updateMusic(game, xr && !xr->focused());
                for (auto sound : game.sounds)
                    audio->play(sound);
            }
            game.sounds.clear();
            if (xr) {
                try {
                    if (!xr->frame(renderer, game)) {
                        xr.reset();
                        if (game.mode == Mode::Playing || game.mode == Mode::Transition ||
                            game.mode == Mode::Bonus)
                            game.pause();
                        status = "XR SESSION ENDED / DESKTOP ACTIVE";
                        SDL_GL_SetSwapInterval(1);
                    }
                } catch (const std::exception &e) {
                    std::cerr << "XR session ended: " << e.what() << '\n';
                    xr.reset();
                    if (game.mode == Mode::Playing || game.mode == Mode::Transition ||
                        game.mode == Mode::Bonus)
                        game.pause();
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
                    renderer.hud(game, width, height, status, settings.sensitivity,
                                 settings.inverted, settings.muted);
                    renderer.finishFrame(game, 4, 0, 0, width, height);
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
            bool wheel = smokeAfter == game.boundedLane(smokeBefore + 2);
            std::cout << "{\"frames\":" << frame << ",\"wheel_before\":" << smokeBefore
                      << ",\"wheel_after\":" << smokeAfter
                      << ",\"wheel_ok\":" << (wheel ? "true" : "false")
                      << ",\"score\":" << game.score << ",\"gl_error\":" << error << "}\n";
            if (!wheel || error != GL_NO_ERROR)
                throw std::runtime_error("Smoke validation failed");
        }
        saveSettings();
        xr.reset();
        audio.reset();
        renderer.releaseGL();
        SDL_GL_DeleteContext(context);
        window.reset();
        SDL_Quit();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "tempest-xr: " << e.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
