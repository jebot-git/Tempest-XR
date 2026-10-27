#include "campaign.hpp"
#include "game.hpp"
namespace tempest {
namespace {
constexpr float tau = 6.283185307f;
}
void Game::startTunnel() {
    tunnelHead = tunnelSteps = 0;
    tunnelPhase = 0;
    tunnelBend = 0;
    tunnelClock = 0;
    tunnelTrail = 3;
    tunnelVelocity = tunnelResultTime = 0;
    bonusSpeed = 60;
    tunnelTrailAngles.clear();
    // itunnel: two patterned bands, and the green track initially at phase zero.
    for (size_t i = 0; i < tunnel.size(); ++i) {
        tunnel[i].track = 0;
        tunnel[i].bands = {TunnelBand{int(i), 32, 255}, TunnelBand{int(i * 2), 48, 176}};
    }
}
float Game::tunnelTrackAngle(int ahead) const {
    return float(tunnel[size_t((tunnelHead + 8 + std::clamp(ahead, 0, 55)) & 63)].track) * tau /
           256;
}
bool Game::tunnelOnTrack() const {
    int phase = int(std::floor(bonusAngle * 256 / tau)) & 255;
    int target = tunnel[size_t((tunnelHead + 8) & 63)].track;
    int distance = std::abs(phase - target);
    // Literal xono/xono2 wrap: the source uses 255-distance, not 256-distance.
    if (distance > 127)
        distance = 255 - distance;
    return distance <= 32;
}
void Game::tickTunnel(float dt) {
    tunnelClock += dt;
    while (tunnelClock + 1e-9 >= 1.0 / 60 && !bonusFinished) {
        tunnelClock -= 1.0 / 60;
        bonusAngle += tunnelVelocity / 60;
        // run_pgens: independent bouncing colour vectors and phase/density clocks.
        if (--tunnelPatternCountdown < 0) {
            tunnelPatternCountdown = 800;
            for (auto &p : tunnelPatterns) {
                p.vx = (int(randf(0, 16)) + 1) * 4;
                p.vy = (int(randf(0, 8)) + 1) * 4;
                p.phaseStep = uint16_t(int(randf(0, 256)) * 2);
                p.densityStep = uint16_t(int(randf(0, 128)) + 16);
            }
        }
        tunnelPhase = uint16_t(int(tunnelPhase) + tunnelBend);
        auto &segment = tunnel[size_t(tunnelSteps & 63)];
        segment.track = tunnelPhase >> 8;
        for (size_t i = 0; i < tunnelPatterns.size(); ++i) {
            auto &p = tunnelPatterns[i];
            p.x += p.vx;
            if (p.x < 0 || p.x > 4095) {
                p.vx = -p.vx;
                p.x += p.vx;
            }
            p.y += p.vy;
            if (p.y < 0 || p.y >= 4095) {
                p.vy = -p.vy;
                p.y += p.vy;
            }
            p.phase = uint16_t(p.phase + p.phaseStep);
            p.density = uint16_t(p.density + p.densityStep);
            segment.bands[i] = {int(p.phase >> 8), std::abs(int((p.density >> 8) & 63) - 31) + 16,
                                (p.x >> 8) | ((p.y >> 4) & 240)};
        }
        tunnelHead = tunnelSteps & 63;
        if ((tunnelSteps & 15) == 15) {
            tunnelBend = tunnelCourses[size_t(bonusCourse)][size_t(bonusGate)] * 64;
            ++bonusGate;
        }
        if (tunnelOnTrack()) {
            constexpr int awards[] = {1, 4, 8, 30};
            bonusLastScore = awards[std::clamp(int(tunnelTrail) / 8, 0, 3)];
            addScore(bonusLastScore);
            ++bonusHits;
            if (tunnelTrail + .5f <= float(0x1fffff) / 65536)
                tunnelTrail += .5f;
        } else {
            bonusLastScore = 0;
            float shortened = tunnelTrail - 1;
            if (shortened < 0 && bonusGate < 64) {
                bonusFinished = true;
                bonusResultTime = 183.f / 60; // 33-frame collapse and 150-frame melt.
                announce("TRAIL LOST", bonusResultTime);
            } else
                tunnelTrail = std::max(0.f, shortened);
        }
        tunnelTrailAngles.insert(tunnelTrailAngles.begin(), bonusAngle);
        if (tunnelTrailAngles.size() > 32)
            tunnelTrailAngles.pop_back();
        ++tunnelSteps;
        bonusDistance = float(tunnelSteps);
        if (bonusGate >= 64) {
            bonusWon = bonusFinished = true;
            bonusResultTime = 5;
            announce(bonusBaseLevel < 90 ? "WARP 5 LEVELS" : "EXCELLENT", 5);
            sounds.push_back(Sound::Excellent);
        }
    }
}
} // namespace tempest
