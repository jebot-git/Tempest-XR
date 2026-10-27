#include "cry.hpp"
#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::to_string(__LINE__) + ": " #c);                          \
    } while (false)
static void frames(Game &g, int n) {
    for (int i = 0; i < n; ++i)
        g.tick(1.f / 60);
}
static Game ready() {
    Game g(44);
    g.start();
    g.invulnerable = 1000;
    return g;
}
static int count(const Game &g, Kind k) {
    return int(std::count_if(g.enemies.begin(), g.enemies.end(),
                             [=](const auto &e) { return !e.dead && e.kind == k; }));
}
int main() {
    try {
        {
            auto g = ready();
            g.fireHeld = true;
            frames(g, 2);
            CHECK(g.shots.empty());
            g.fireHeld = false;
            frames(g, 20);
            CHECK(g.shots.empty());
            g.fireHeld = true;
            frames(g, 1);
            CHECK(g.shots.empty());
            frames(g, 1);
            CHECK(g.shots.size() == 1 && g.shots[0].depth == 0);
            frames(g, 1);
            CHECK(g.shots[0].depth > 0);
            g.fireHeld = false;
            auto fired = g.shots.size();
            frames(g, 2);
            CHECK(g.shots.size() == fired);
        }
        for (int phase : {1, 2, 3}) {
            auto g = ready();
            frames(g, phase);
            g.spawn(Kind::Flipper, 0, .7f);
            g.zap();
            frames(g, 3 - phase);
            CHECK(count(g, Kind::Flipper) == 1);
            frames(g, 1);
            CHECK(count(g, Kind::Flipper) == 0 && g.frameNumber() == 4);
        }
        // run_flipper's zap bypasses turning/climbing hit windows, including beasts.
        for (Kind kind : {Kind::Flipper, Kind::Beast, Kind::Fuseball}) {
            auto g = ready();
            g.spawn(kind, 2, .5f);
            g.enemies[0].motion = FlipperMotion::Turning;
            g.enemies[0].turnArc = 128;
            g.enemies[0].turnAngle = 0;
            g.zap();
            frames(g, 4);
            CHECK(count(g, kind) == 0);
        }
        // Spark contact precedes its collie call, even on a zap frame.
        {
            auto g = ready();
            frames(g, 3);
            g.invulnerable = 0;
            g.spawn(Kind::PulsarSpark, g.lane, 0);
            g.zap();
            frames(g, 1);
            CHECK(g.lives == 2 && count(g, Kind::PulsarSpark) == 1);
        }
        for (bool reverse : {false, true})
            for (bool rimNewest : {false, true}) {
                auto g = ready();
                g.invulnerable = 0;
                Enemy rim{Kind::Flipper, g.lane, 0};
                rim.rim = true;
                rim.motion = FlipperMotion::Stopped;
                rim.stopTime = 1;
                rim.serial = rimNewest ? 20 : 10;
                Enemy behind{Kind::Flipper, g.lane, .025f};
                behind.serial = rimNewest ? 10 : 20;
                g.enemies = {rim, behind};
                if (reverse)
                    std::reverse(g.enemies.begin(), g.enemies.end());
                Shot shot{g.lane};
                shot.serial = 30;
                g.shots = {shot};
                frames(g, 1);
                CHECK(g.lives == (rimNewest ? 3 : 2));
                CHECK(g.score == 150);
            }
        {
            auto g = ready();
            g.spawn(Kind::Flipper, 0, 1, true);
            frames(g, 149);
            CHECK(g.enemies[0].entering);
            frames(g, 1);
            CHECK(!g.enemies[0].entering && g.enemies[0].depth < 1);
        }
        {
            auto g = ready();
            g.startBonus(0);
            CHECK(std::abs(g.bonusGateDistance(0) - 269.f) < .001f);
            frames(g, 41);
            int32_t counter = g.bonusGeneratorPhase();
            CHECK(counter != 0x100000);
            g.startBonus(2);
            CHECK(g.bonusGeneratorPhase() == counter);
            float step = g.bonusSpeed / 60;
            int n = 0;
            int32_t copy = counter;
            do {
                ++n;
                copy -= int32_t(std::round(step * 65536));
            } while (copy >= 0);
            CHECK(std::abs(g.bonusGateDistance(0) -
                           (n * step + (std::floor(252 / step) + 1) * step)) < .001f);
        }
        {
            auto g = ready();
            g.showMessage("TEST", 100);
            frames(g, 101);
            CHECK(g.messageTime > 0 && g.messageScaleY == 1);
            frames(g, 1);
            CHECK(g.messageScaleY == 1.5f);
            CHECK(std::abs(g.messageShearX -
                           (std::abs(int((g.frameNumber() * 2) & 255) - 127) - 63) / 64.f) <
                  .00001f);
            frames(g, 50);
            CHECK(g.messageTime == 0);
            g.showMessage("AVOID THE SPIKES", 250);
            frames(g, 251);
            CHECK(g.messageScaleY == 1);
        }
        {
            auto g = ready();
            g.startBonus(0);
            g.rotary(1);
            CHECK(g.rotaryMultiplier() == 1.5f);
            frames(g, 1);
            g.rotary(1);
            CHECK(g.rotaryMultiplier() == 2);
            frames(g, 1);
            g.rotary(1);
            CHECK(g.rotaryMultiplier() == 2.5f);
            frames(g, 1);
            frames(g, 24);
            CHECK(g.rotaryMultiplier() == 1);
            g.strictRotaryLimits = true;
            g.bonusY = 1.251f;
            g.bonusUp = true;
            g.bonusVerticalVelocity = 1;
            frames(g, 1);
            CHECK(g.bonusVerticalVelocity == 0 && g.bonusY == 1.251f);
            g.bonusY = -1.251f;
            g.bonusUp = false;
            g.bonusDown = true;
            g.bonusVerticalVelocity = -1;
            frames(g, 1);
            CHECK(g.bonusVerticalVelocity == 0 && g.bonusY == -1.251f);
            g.bonusDown = false;
            g.bonusHorizontalVelocity = 0;
            g.secondaryPad(1, 0);
            float x = g.bonusX;
            frames(g, 4);
            CHECK(g.bonusX < x);
        }
        CHECK(shadeCry(0x84fa, 20) == 0x84ff && shadeCry(0x8404, -8) == 0x8400);
        {
            Game g;
            auto scores = g.highScores;
            frames(g, 501);
            CHECK(g.frontPage == FrontPage::Scores);
            frames(g, 501);
            CHECK(g.frontPage == FrontPage::Credits);
            frames(g, 501);
            CHECK(g.frontPage == FrontPage::Tutorial);
            frames(g, 502);
            CHECK(g.attractGame && g.frontPage == FrontPage::Demo);
            frames(g, 240);
            CHECK(g.attractGame->frameNumber() > 100);
            CHECK(g.score == 0 && g.highScores[0].score == scores[0].score);
            g.unlockedLevel = 99;
            g.returnToTitle();
            g.steer(50);
            CHECK(g.selectedLevel == 1);
            auto globalFrame = g.frameNumber();
            g.activate();
            CHECK(g.level == 1 && g.frameNumber() == globalFrame);
        }
        std::cout << "Source frame order, held firing, global zap, gate carry, message phases, "
                     "rotary limits/calibration, CRY and frontend passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
