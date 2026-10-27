#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
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
    PulsarSpark,
    MirrorShot,
    BeastShot
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
    FlipperLand,
    TankerOpen,
    MirrorReflect,
    AlienBonus,
    Spike,
    PowerupSpawn
};
struct SoundCue {
    Sound sound;
    int pitch = 0;
    SoundCue(Sound sound, int pitch = 0) : sound(sound), pitch(pitch) {}
    operator Sound() const {
        return sound;
    }
};
enum class SpikerMotion { Seeking, Climbing, Descending };
enum class FuseMotion { Climbing, Crossing };
enum class DroidMotion { Climbing, Starting, Crossing, Zapping };
enum class FlipperMotion { Climbing, Turning, Stopped };
struct Enemy {
    Kind kind;
    int lane;
    float depth = 1, age = 0;
    int direction = 1;
    bool rim = false, dead = false;
    bool emerging = false, entering = false, ally = false;
    uint64_t serial = 0;
    int pickupFrames = 0;
    int rail = -1;
    int hp = 1;
    float sparkTime = 5.f / 60;
    FlipperMotion motion = FlipperMotion::Climbing;
    SpikerMotion spikerMotion = SpikerMotion::Seeking;
    FuseMotion fuseMotion = FuseMotion::Climbing;
    DroidMotion droidMotion = DroidMotion::Climbing;
    double aiClock = 0, fireClock = 0;
    int buildFrames = 50, riseFrames = 48, crossFrames = 1, crossSteps = 15;
    int droidFrames = 2, droidSteps = 16, zapFrames = 32, scarper = 0;
    int travelFrom = -1, travelTo = -1;
    bool runningBelowRim = false;
    int turnFrom = -1;
    float turnAngle = 0, turnArc = 128, stopTime = 0;
};
struct Shot {
    int lane;
    float depth = 0, previous = 0;
    bool dead = false;
    bool ally = false, takenOver = false;
    uint64_t serial = 0, bornFrame = UINT64_MAX, creatorSerial = 0;
    int tableSlot = -1;
};
enum class EffectKind { Rings, Sphere, Splatter, Firework, Pickup, Score };
struct Effect {
    EffectKind kind;
    Vec3 pos, velocity{};
    int frame = 0, duration = 50, color = 0, density = 1;
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
struct TunnelBand {
    int phase = 0, pixels = 32, color = 255;
};
struct TunnelSegment {
    int track = 0;
    std::array<TunnelBand, 2> bands{};
};
struct ScoreEntry {
    std::string initials;
    int score = 0, level = 1;
    bool beastly = false;
};
struct AccessKey {
    int unlocked = 0;
    bool beastly = false;
    std::string initials = "...";
};
enum class FrontPage {
    Title,
    Scores,
    Credits,
    Tutorial,
    Demo,
    Keys,
    GameOver,
    Congratulations,
    Initials
};
class Game {
    std::mt19937 random;
    int fireFrames = 3, nextBulletSlot = 0;
    bool bulletExpanded = false;
    int claimBulletSlot();
    double sourceClock = 0;
    uint64_t sourceFrame = 0, nextSerial = 1, droidSerial = 0;
    void tickStep(float dt);
    float transitionTimer = 0, wheelRemainder = 0;
    int kills = 0, nextLife = 20000;
    Mode pausedFrom = Mode::Playing;
    Vec3 nextSpawn;
    Vec3 deathShip;
    Vec3 respawnShip;
    Vec3 transitionOffset, bonusEntryOffset;
    Vec3 returnOffset;
    float returnTime = 0, rechargeDelay = 0;
    int pendingBonus = -1, bonusBaseLevel = 1;
    std::vector<int> spawnTimers;
    std::vector<uint64_t> spikeSerials;
    double generationClock = 0, droidClock = 0;
    uint64_t zapLastFrame = 0;
    int droidSteps = 0, droidFrom = 0, droidTo = 0, droidFireFrames = 5;
    bool zapActive = false, zapSingle = false, infiniteZap = false;
    double effectClock = 0;
    int fireworkWait = 1, fireworkDuration = 80, fireworkColor = 0;
    size_t fireworkPC = 0;
    Vec3 fireworkPosition{}, fireworkVelocity{};
    std::vector<std::pair<size_t, int>> fireworkStack;
    void tickEffects(float dt);
    void tickFireworks();
    void surprisePowerup();
    void offerHighScore();
    void tickFrontend(float dt, bool sourceUpdate);
    bool pendingScore = false;
    int runHighest = 1, keyCursor = 0;
    float frontAge = 0;
    int frontTicks = 0;
    void awardKey();
    void startZap(bool single = false, bool infinite = false);
    void tickZap(float dt);
    bool zapEligible(const Enemy &e) const;
    void tickDroid(float dt);
    void animateClearance(float dt);
    std::vector<Enemy> pending;
    int killsToPowerup = 1, bonusCourse = 0;
    std::array<int, 3> bonusRuns{};
    float bonusResultTime = 0;
    int32_t gateCounter = 0x100000;
    int gateTimer = 0, gateNext = 0;
    double bonusClock = 0;
    std::vector<int64_t> gateLaunches;
    int32_t bonusVelocityFixed = 0x8000;
    int64_t bonusGroundFixed = 0;
    void planBonusGates();
    void advanceBonusGenerator();
    int rotarySensitivity = 0x2000;
    bool rotaryMoved = false;
    unsigned secondaryHorizontal = 0, secondaryVertical = 0;
    bool secondaryConnected = false;
    float padHorizontalVelocity = 0;
    void tickRotaryCalibration();
    bool lastPulse = false;
    int alienFireCounter = 112;
    struct TunnelPattern {
        int x, vx, y, vy;
        uint16_t phase, phaseStep, density, densityStep;
    };
    std::array<TunnelPattern, 2> tunnelPatterns{{{128, 22, 64, 28, 0x2000, 0x100, 0x800, 0x60},
                                                 {80, 9, 112, 15, 0x6000, 0x280, 0xc00, 0xc0}}};
    int tunnelPatternCountdown = 300, tunnelBend = 0;
    uint16_t tunnelPhase = 0;
    double tunnelClock = 0, bonusControlClock = 0;
    bool bonusControlStarted = false;
    void tickRotaryBonus(float dt);
    void startTunnel();
    void tickTunnel(float dt);
    float randf(float lo, float hi);
    void loadWeb();
    void hit(bool snatch = false);
    void destroy(Enemy &e, bool zap = false);
    void announce(const std::string &text, float seconds = 1.6f);
    void collectPowerup();
    void beginTransition(int destination);
    void completeWave();
    void finishCampaign();
    void tickBonus(float dt);
    void tickPickups(float dt);
    void returnFromBonus();
    int directionTo(int from, int to) const;
    void startFlipperTurn(Enemy &e);
    void beginFlipperTurn(Enemy &e, int direction);
    void openTanker(Enemy &e, std::vector<Enemy> &children);
    float flipRotation(const Enemy &e) const;
    static Enemy makeEnemy(Kind kind, int lane, float depth, int direction);
    float enemySpeed(Kind kind) const;
    void beginFuseCrossing(Enemy &e, int direction);
    void tickSpecialEnemy(Enemy &e, float dt);
    void tryAlienFire(Enemy &e, float dt, std::vector<Enemy> &children);

  public:
    Mode mode = Mode::Menu;
    std::array<ScoreEntry, 10> highScores{{{"YAK", 500017},
                                           {"EWE", 400000},
                                           {"COW", 300000},
                                           {"GNU", 200000},
                                           {"OX ", 100000},
                                           {"ELK", 90000},
                                           {"DOE", 80000},
                                           {"MOO", 70000},
                                           {"BAA", 60000},
                                           {"FUR", 50000}}};
    std::array<AccessKey, 4> accessKeys{};
    inline static constexpr const char *keyNames[] = {"YAK", "COW", "ZOO", "AXE"};
    int accessKey = -1, recordsVersion = 0;
    FrontPage frontPage = FrontPage::Title;
    std::shared_ptr<Game> attractGame;
    int scoreChoice = 0;
    float scoreWheelPosition = 0;
    void activate();
    void returnToTitle();
    std::vector<std::string> frontendLines() const;
    int frontendFrame() const {
        return frontTicks;
    }
    int savedKeyCursor() const {
        return keyCursor;
    }
    static int checkpoint(int reached) {
        return std::max(1, ((std::min(reached, 99) - 2) & ~1) + 1);
    }
    bool enteringScore = false, enteringBrag = false;
    std::string brag = "BEAST";
    std::string scoreInitials = "AAA";
    int scoreCursor = 0;
    void finishScoreEntry();
    void nextScoreLetter();
    void setScoreLetter(char letter);
    void storeAccessKey();
    void selectAccessKey();
    Web web;
    int level = 1, score = 0, best = 0, lives = 3, lane = 0, zaps = 2;
    int targetLevel = 2, enemiesRemaining = 0, waveSize = 0, unlockedLevel = 1, selectedLevel = 1;
    int powerupStage = 0, bonusTokens = 0, bonusType = 0, bonusGate = 0, bonusHits = 0,
        bonusesPlayed = 0;
    double waveTime = 0;
    float time = 0, visualLane = 0, jumpTime = 0, jumpCooldown = 0, invulnerable = 0, flash = 0;
    bool fireHeld = false, boost = false, strictRotaryLimits = false;
    float bonusVerticalLimit() const;
    bool jumpEnabled = false, droidActive = false, bonusUp = false, bonusDown = false,
         bonusWon = false, bonusFinished = false;
    bool droidPending = false, earlyDroid = false;
    float droidLane = 0, bonusTime = 0, bonusX = 0, bonusY = 0, bonusAngle = 0;
    float bonusDistance = 0, bonusSpeed = 30, bonusVerticalVelocity = 0, bonusEffectTime = 0;
    float bonusHorizontalVelocity = 0;
    static constexpr float ringWheelGain = .125f * 2 * 60 / 32;
    static constexpr float bonusVerticalMax = 8.f * 60 / 256;
    int bonusBoosts = 0, bonusLastScore = 0;
    std::array<TunnelSegment, 64> tunnel{};
    std::vector<float> tunnelTrailAngles;
    int tunnelHead = 0, tunnelSteps = 0;
    float tunnelTrail = 3, tunnelVelocity = 0, tunnelResultTime = 0;
    static constexpr float tunnelWheelGain = .8f;
    float tunnelTrackAngle(int ahead = 0) const;
    bool tunnelOnTrack() const;
    float tunnelInterpolation() const {
        return std::clamp(float(tunnelClock * 60), 0.f, 1.f);
    }
    float deathTime = 0, oneUpTime = 0;
    bool beastly = false, beastlyUnlocked = false, earlyExit = false;
    bool clearanceWarning() const {
        return level >= 4 && level <= 16 && !earlyExit;
    }
    size_t shotSlots(bool ally) const;
    int objectSlots() const;
    int physicalSlots() const;
    bool canGenerate() const;
    bool snatched = false;
    static constexpr float deathDuration = 1.6f;
    float deathProgress() const {
        return deathTime > 0 ? 1 - deathTime / deathDuration : 1;
    }
    Vec3 deathOrigin() const {
        return deathShip;
    }
    int enginePitch = 10, engineVolumePhase = 0;
    float zapTime = 0, messageTime = 0, messageDuration = 1;
    int messageFrame = 0, messageHold = 100;
    float messageScaleX = 1, messageScaleY = 1, messageShearX = 0, messageShearY = 0;
    void showMessage(const std::string &text, int hold = 100);
    void rotary(float notches);
    float rotaryMultiplier() const {
        return float(rotarySensitivity) / 0x2000;
    }
    void secondaryPad(unsigned horizontal, unsigned vertical, bool connected = true) {
        secondaryConnected = connected;
        secondaryHorizontal = horizontal & 3;
        secondaryVertical = vertical & 3;
    }
    std::string message;
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<float> spikes; // Lane tip depth; 1 means absent unless spikePresent is set.
    std::vector<bool> spikePresent, superSpikes;
    bool hasSpike(int l) const {
        return spikePresent[size_t(l)] || spikes[size_t(l)] < 1;
    }
    static bool reflected(Kind kind) {
        return kind == Kind::MirrorShot || kind == Kind::BeastShot;
    }
    uint64_t frameNumber() const {
        return sourceFrame;
    }
    int pulsePhase() const;
    float shotSpeed() const {
        return (boost ? 4.5f : 3.5f) * 60 / 160 * (beastly ? .5f : 1.f);
    }
    std::vector<Effect> effects;
    std::vector<Particle> particles;
    std::vector<SoundCue> sounds;
    explicit Game(uint32_t seed = 2000);
    void start(int startingLevel = 1, bool awardStart = false);
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
    void spawn(Kind kind, int spawnLane = -1, float depth = 1, bool entering = false);
    static Kind kindForSource(int type);
    static bool hostile(Kind kind);
    static bool flipperKind(Kind kind);
    static bool flipperBody(const Enemy &e) {
        return flipperKind(e.kind) || e.emerging;
    }
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
    int32_t bonusGeneratorPhase() const {
        return gateCounter;
    }
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
