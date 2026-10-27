#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #c);                                                      \
    } while (false)
static void steps(Game &g, int count) {
    for (int i = 0; i < count; ++i)
        g.tick(1.f / 60);
}
int main() {
    try {
        Game g;
        g.start();
        g.startBonus(1);
        CHECK(g.tunnelTrail == 3 && g.tunnelOnTrack() && g.tunnelSteps == 0);
        steps(g, 10);
        CHECK(g.tunnelSteps == 10 && g.score == 10 && g.tunnelTrail == 8);
        steps(g, 1);
        CHECK(g.score == 14 && g.tunnelTrail == 8.5f);
        steps(g, 49);
        CHECK(g.tunnelSteps == 60 && g.tunnelTrail == 31.5f);
        g.pause();
        int before = g.tunnelSteps, score = g.score;
        auto camera = g.xrCameraPosition();
        g.tick(1);
        CHECK(g.tunnelSteps == before && g.score == score && camera.x == g.xrCameraPosition().x);
        g.pause();
        g.start();
        g.startBonus(1);
        g.bonusAngle = 1.570796327f;
        steps(g, 3);
        CHECK(!g.bonusFinished && g.tunnelTrail == 0 && g.lives == 3);
        steps(g, 1);
        CHECK(g.bonusFinished && !g.bonusWon && g.lives == 3 && g.tunnelSteps == 4);
        g.start();
        g.startBonus(1);
        steps(g, 3);
        CHECK(g.tunnelTrail == 4.5f);
        g.bonusAngle = 1.570796327f;
        steps(g, 1);
        CHECK(g.tunnelTrail == 3.5f && !g.bonusFinished);
        g.bonusAngle = 0;
        steps(g, 1);
        CHECK(g.tunnelTrail == 4 && g.score == 4);
        constexpr float tau = 6.283185307f;
        for (auto entry : {std::pair{32.25f, true}, std::pair{33.25f, true},
                           std::pair{-32.25f, false}, std::pair{-33.25f, false}}) {
            g.start();
            g.startBonus(1);
            g.bonusAngle = entry.first * tau / 256;
            CHECK(g.tunnelOnTrack() == entry.second);
        }
        g.start();
        g.startBonus(1);
        g.steer(1);
        steps(g, 1);
        float moved = g.bonusAngle;
        steps(g, 1);
        CHECK(g.bonusAngle > moved);
        g.steer(-1);
        moved = g.bonusAngle;
        steps(g, 1);
        CHECK(g.tunnelVelocity == 0 && g.bonusAngle == moved);
        // Complete all eight source tracks using visible track angle + wheel impulses.
        g.start();
        for (int course = 0; course < 8; ++course) {
            g.startBonus(1);
            int initial = g.score, ticks = 0;
            while (!g.bonusFinished && ticks++ < 120 * 30) {
                float error = std::remainder(g.tunnelTrackAngle() - g.bonusAngle, tau);
                float desired = std::clamp(error * 8.f, -6.f, 6.f);
                g.steer((desired - g.tunnelVelocity) / Game::tunnelWheelGain);
                g.tick(1.f / 120);
                g.sounds.clear();
            }
            CHECK(g.bonusWon && g.bonusGate == 64 && g.tunnelSteps == 1024 && g.bonusHits == 1024);
            CHECK(g.score - initial ==
                  29662); // Source ltail score bands; no invented completion award.
            CHECK(ticks == 2048);
            CHECK(g.tunnelTrail == 31.5f);
            bool patterned = false, curved = false;
            for (auto &s : g.tunnel) {
                curved |= s.track != 0;
                patterned |= s.bands[0].pixels != 32;
            }
            CHECK(patterned && curved);
        }
        std::cout << "Original tunnel track/trail rules, tolerance, recovery, scoring, inertia, "
                     "pause and eight courses passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
