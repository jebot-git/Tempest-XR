#include "meshes.hpp"
#include "raster_data.hpp"
#include "render.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace tempest;
int main() {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    try {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        window = SDL_CreateWindow("XR render validation", 0, 0, 512, 512,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        if (!window)
            throw std::runtime_error(SDL_GetError());
        context = SDL_GL_CreateContext(window);
        if (!context)
            throw std::runtime_error(SDL_GetError());
        Renderer renderer;
        if (!Renderer::originalArtAvailable())
            throw std::runtime_error("Missing original CRY atlases");
        for (const auto &font : sourceFonts)
            if (font.glyphs.size() != 96)
                throw std::runtime_error("Incomplete original font map");
        Game game;
        Game status;
        auto expectHud = [&](const std::string &fragment, bool present = true) {
            auto lines = Renderer::xrHudLines(status);
            bool found = std::any_of(lines.begin(), lines.end(), [&](const auto &line) {
                return line.find(fragment) != std::string::npos;
            });
            if (found != present)
                throw std::runtime_error("Incorrect XR status: " + fragment);
        };
        status.unlockedLevel = 17;
        status.selectedLevel = 12;
        expectHud("START WEB 12 / UNLOCKED 17");
        status.start();
        status.zaps = 1;
        status.bonusTokens = 2;
        expectHud("ZAP 1");
        expectHud("TOKENS 2/3");
        expectHud("JUMP LOCKED");
        status.jumpEnabled = true;
        expectHud("JUMP READY");
        status.jumpCooldown = 1.5f;
        expectHud("RECHARGING 1.5S");
        status.boost = status.droidActive = true;
        expectHud("LASER ON   DROID ON");
        status.startBonus(2);
        status.pause();
        expectHud("PSYCHEDELIC BONUS");
        expectHud("HOLD LMB / UP / W: UP");
        expectHud("MISSING ENDS BONUS");
        expectHud("PAUSED / CLICK OR ENTER TO RESUME");
        status.startBonus(1);
        expectHud("TURNS 0/64");
        expectHud("TRAIL 3.0/31.5");
        expectHud("STAY ON THE GREEN TRACK");
        expectHud("OFF TRACK SHRINKS IT");
        expectHud("MISSING ONE", false);
        status.mode = Mode::Victory;
        expectHud("CAMPAIGN COMPLETE");
        expectHud("PLAY AGAIN");
        expectHud("AVOID SPIKES", false);
        std::vector<unsigned char> pixels(512 * 512 * 3);
        auto render = [&](float yaw, float pitch) {
            glViewport(0, 0, 512, 512);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum(-.05, .05, -.05, .05, .05, 160);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glRotatef(pitch, 1, 0, 0);
            glRotatef(yaw, 0, 1, 0);
            renderer.world(game, true);
            renderer.finishFrame(game, 0, 0, 0, 512, 512);
            glReadPixels(0, 0, 512, 512, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("OpenGL error in XR renderer");
        };
        const std::array<std::array<float, 2>, 6> directions = {
            {{0, 0}, {90, 0}, {180, 0}, {270, 0}, {0, 90}, {0, -90}}};
        for (auto direction : directions) {
            render(direction[0], direction[1]);
            std::array<int, 4> quadrants{};
            for (int y = 0; y < 512; y++)
                for (int x = 0; x < 512; x++) {
                    size_t p = size_t(y * 512 + x) * 3;
                    if (pixels[p] > 30 || pixels[p + 1] > 30 || pixels[p + 2] > 30)
                        quadrants[size_t((y >= 256) * 2 + (x >= 256))]++;
                }
            for (int count : quadrants)
                if (count < 30)
                    throw std::runtime_error("Empty XR sky quadrant");
            std::cout << "Sky yaw=" << direction[0] << " pitch=" << direction[1] << " quadrants=";
            for (int count : quadrants)
                std::cout << count << ' ';
            std::cout << '\n';
        }
        game.start();
        game.zap();
        render(0, 0);
        game.jumpEnabled = true;
        game.jump();
        for (int i = 0; i < 45; i++)
            game.tick(1.f / 120);
        render(0, 0);
        game.boost = true;
        game.burst(game.shipPosition(), 3, 200);
        render(0, 0);
        game.enemies.clear();
        game.enemiesRemaining = 0;
        for (int i = 0; i < 250; i++) {
            game.tick(1.f / 120);
            render(0, 0);
        }
        for (const auto &mesh : sourceMeshes) {
            for (const auto &face : mesh.faces) {
                if (face.vertices.size() < 3)
                    throw std::runtime_error("Invalid original mesh face");
                for (auto v : face.vertices)
                    if (!std::isfinite(v.x) || !std::isfinite(v.y) || v.light < 0 || v.light > 1)
                        throw std::runtime_error("Invalid original mesh intensity");
            }
        }
        for (int level : {9, 17, 100}) {
            game.start(level);
            game.enemiesRemaining = 0;
            game.fireHeld = false;
            for (int type = 0; type < 12; type++)
                game.spawn(Game::kindForSource(type), type % game.web.lanes(), .4f);
            game.spawn(Kind::MirrorShot, 0, .3f);
            game.spawn(Kind::BeastShot, 2, .4f);
            game.superSpikes[3] = true;
            game.spikes[3] = .5f;
            game.droidActive = true;
            for (float age : {0.f, .11f, .22f, .33f}) {
                for (auto &enemy : game.enemies)
                    enemy.age = age;
                render(0, 0);
            }
        }
        for (int type = 0; type < 3; type++) {
            game.startBonus(type);
            for (int i = 0; i < 4; i++) {
                game.tick(.1f);
                render(0, 0);
            }
            game.pause();
            render(0, 0);
            game.pause();
        }
        // Actual pivoted rim turns on flat, circular and concave webs.
        for (int level : {1, 3, 10}) {
            game.start(level);
            game.fireHeld = false;
            game.invulnerable = 100;
            Enemy flipper{Kind::Flipper, game.boundedLane(game.lane - 1), 0};
            flipper.rim = true;
            game.enemies = {flipper};
            for (int frame = 0; frame < 100; ++frame) {
                game.tick(1.f / 120);
                if (frame % 10 == 0)
                    render(0, 0);
            }
        }
        game.start(100);
        game.enemies.clear();
        game.enemiesRemaining = 0;
        game.tick(.01f);
        render(0, 0);
        for (auto kind : {Kind::Flipper, Kind::EnemyShot}) {
            game.start();
            game.fireHeld = false;
            game.invulnerable = 0;
            game.spawn(kind, game.lane, .0001f);
            for (int i = 0; i < 190; ++i) {
                game.tick(1.f / 120);
                if (i % 20 == 0)
                    render(0, 0);
            }
        }
        renderer.releaseGL();
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "Full-sphere sky, jump, zap, particles, messages, and warp render without GL "
                     "errors\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        if (context)
            SDL_GL_DeleteContext(context);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
