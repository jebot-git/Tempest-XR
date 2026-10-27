#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace tempest {
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 b) const {
        return {x + b.x, y + b.y, z + b.z};
    }
    Vec3 operator-(Vec3 b) const {
        return {x - b.x, y - b.y, z - b.z};
    }
    Vec3 operator*(float s) const {
        return {x * s, y * s, z * s};
    }
};
enum class Mode { Menu, Playing, Paused, Transition, GameOver, Bonus, Victory, BonusReturn };
enum class Kind {
    Flipper,
    Tanker,
    Spiker,
    Powerup,
    Fuseball,
    Pulsar,
    FuseTanker,
    PulsarTanker,
    SuperFlipper,
    SuperFlipper2,
    Mirror,
    Adroid,
    Beast,
    EnemyShot,
    PulsarSpark
};
enum class Sound {
    Shot,
    Kill,
    Hit,
    Jump,
    Zap,
    Wave,
    Pickup,
    Warp,
    PoweredShot,
    Excellent,
    OneUp,
    Pulse,
    Ricochet,
    GateYes,
    GateYesAlt,
    Snatch,
    Yes,
    ClearanceEngine,
    BonusToken,
    FlipperLand
};
enum class FlipperMotion { Climbing, Turning, Stopped };
struct Enemy {
    Kind kind;
    int lane;
    float depth = 1, age = 0, flip = 0;
    int direction = 1;
    bool rim = false, dead = false;
    bool retreating = false;
    int hp = 1;
    float firing = 0;
    FlipperMotion motion = FlipperMotion::Climbing;
    int turnFrom = -1;
    float turnAngle = 0, turnArc = 128, stopTime = 0;
};
struct Shot {
    int lane;
    float depth = 0, previous = 0;
    bool dead = false;
    bool ally = false;
};
struct Particle {
    Vec3 pos, velocity;
    float life, maxLife;
    int color;
};
struct Web {
    std::string name;
    std::vector<Vec3> points;
    bool closed;
    int start;
    std::vector<int> orientations;
    int lanes() const {
        return int(points.size()) - (closed ? 0 : 1);
    }
};
class Game {
    std::mt19937 random;
    float fireTimer = 0, transitionTimer = 0, wheelRemainder = 0, droidTimer = 0;
    int kills = 0, nextLife = 20000;
    Mode pausedFrom = Mode::Playing;
    Vec3 nextSpawn;
    Vec3 deathShip;
    Vec3 respawnShip;
    Vec3 transitionOffset, bonusEntryOffset;
    Vec3 returnOffset;
    float returnTime = 0, rechargeDelay = 0;
    int pendingBonus = -1, bonusBaseLevel = 1;
    std::vector<float> spawnTimers;
    std::vector<Enemy> pending;
    int killsToPowerup = 1, bonusCourse = 0;
    std::array<int, 3> bonusRuns{};
    float bonusCheckTime = 0, bonusResultTime = 0;
    bool lastPulse = false;
    float randf(float lo, float hi);
    void loadWeb();
    void hit(bool snatch = false);
    void destroy(Enemy &e, bool zap = false);
    void announce(const std::string &text, float seconds = 1.6f);
    void collectPowerup();
    void beginTransition(int destination);
    void completeWave();
    void tickBonus(float dt);
    void tickPickups(float dt);
    void returnFromBonus();
    int directionTo(int from, int to) const;
    void startFlipperTurn(Enemy &e);
    float flipRotation(const Enemy &e) const;

  public:
    Mode mode = Mode::Menu;
    Web web;
    int level = 1, score = 0, best = 0, lives = 3, lane = 0, zaps = 2;
    int targetLevel = 2, enemiesRemaining = 0, waveSize = 0, unlockedLevel = 1, selectedLevel = 1;
    int powerupStage = 0, bonusTokens = 0, bonusType = 0, bonusGate = 0, bonusHits = 0,
        bonusesPlayed = 0;
    float time = 0, waveTime = 0, visualLane = 0, jumpTime = 0, jumpCooldown = 0, invulnerable = 0,
          flash = 0;
    bool autoFire = true, fireHeld = false, boost = false;
    bool jumpEnabled = false, droidActive = false, bonusUp = false, bonusDown = false,
         bonusWon = false, bonusFinished = false;
    bool droidPending = false, earlyDroid = false;
    float droidLane = 0, bonusTime = 0, bonusX = 0, bonusY = 0, bonusAngle = 0;
    float bonusDistance = 0, bonusSpeed = 30, bonusImpulse = 0, bonusEffectTime = 0;
    int bonusBoosts = 0, bonusLastScore = 0;
    float deathTime = 0, oneUpTime = 0;
    bool snatched = false;
    static constexpr float deathDuration = 1.6f;
    float deathProgress() const {
        return deathTime > 0 ? 1 - deathTime / deathDuration : 1;
    }
    Vec3 deathOrigin() const {
        return deathShip;
    }
    float zapTime = 0, messageTime = 0, messageDuration = 1;
    std::string message;
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<float> spikes; // Lane tip depth; 1 means no spike.
    std::vector<Particle> particles;
    std::vector<Sound> sounds;
    explicit Game(uint32_t seed = 2000);
    void start(int startingLevel = 1);
    void steer(float notches);
    int boundedLane(int n) const;
    void jump();
    void zap();
    void pause();
    void tick(float dt);
    Vec3 edge(int index, float depth) const;
    Vec3 position(float lanePosition, float depth) const;
    void burst(Vec3 p, int color, int count = 25);
    void addScore(int points);
    void spawn(Kind kind, int spawnLane = -1, float depth = 1);
    static Kind kindForSource(int type);
    static bool hostile(Kind kind);
    static bool flipperKind(Kind kind);
    static bool shotVulnerable(const Enemy &e);
    bool isBonus() const {
        return mode == Mode::Bonus || (mode == Mode::Paused && pausedFrom == Mode::Bonus);
    }
    bool isBonusReturn() const {
        return mode == Mode::BonusReturn ||
               (mode == Mode::Paused && pausedFrom == Mode::BonusReturn);
    }
    bool active() const {
        return mode == Mode::Playing || mode == Mode::Transition || mode == Mode::Bonus ||
               mode == Mode::BonusReturn;
    }
    bool pulseActive() const;
    Vec3 enemyPosition(const Enemy &e) const;
    Vec3 enemyTangent(const Enemy &e) const;
    Vec3 bonusTarget(int gate) const;
    Vec3 bonusCursor() const;
    int bonusGateCount() const;
    float bonusGateInterval() const;
    int bonusGateDelay(int gate) const;
    int bonusGateEffect(int gate) const;
    float bonusGateDistance(int gate) const;
    float bonusGateZ(int gate) const;
    void startBonus(int type);
    float progress() const {
        return isBonus() ? float(bonusGate) / std::max(1, bonusGateCount())
                         : 1 - float(enemiesRemaining) / std::max(1, waveSize);
    }
    float transitionDepth() const {
        return (mode == Mode::Transition ||
                (mode == Mode::Paused && pausedFrom == Mode::Transition))
                   ? std::clamp(1 - transitionTimer / 2.4f, 0.f, 1.f)
                   : 0.f;
    }
    static constexpr float warpTravel = 14.f;
    float warpDistance() const {
        float t = transitionDepth();
        return warpTravel * t * t * (3 - 2 * t);
    }
    float jumpLift() const {
        return jumpTime > 0 ? std::sin(jumpTime / .85f * 3.14159265f) * .8f : 0.f;
    }
    Vec3 shipPosition() const;
    Vec3 xrCameraPosition() const {
        if (mode == Mode::Menu)
            return {};
        return shipPosition() + Vec3{0, 0, 3.8f};
    }
};
} // namespace tempest
