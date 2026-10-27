#include "game.hpp"
namespace tempest {
void Game::returnToTitle() {
    finishScoreEntry();
    mode = Mode::Menu;
    frontPage = FrontPage::Title;
    frontAge = 0;
    frontTicks = 0;
    accessKey = -1;
    selectedLevel = 1;
    enteringScore = false;
    fireHeld = false;
    deathTime = 0;
    enemies.clear();
    shots.clear();
    effects.clear();
    attractGame.reset();
}
void Game::activate() {
    if (mode == Mode::Menu) {
        int startLevel = 1;
        accessKey = -1;
        if (frontPage == FrontPage::Keys) {
            const auto &key = accessKeys[size_t(keyCursor)];
            if (!key.unlocked)
                return;
            startLevel = key.unlocked;
            accessKey = keyCursor;
        }
        start(startLevel, true);
        return;
    }
    if (mode == Mode::GameOver || mode == Mode::Victory) {
        if (enteringScore) {
            nextScoreLetter();
            return;
        }
        if (frontPage == FrontPage::GameOver && frontTicks < 11)
            return;
        if (frontPage == FrontPage::Scores) {
            returnToTitle();
            return;
        }
        offerHighScore();
    }
}
void Game::tickFrontend(float dt, bool sourceUpdate) {
    frontAge += dt;
    if (sourceUpdate)
        ++frontTicks;
    if (enteringScore) {
        if (sourceUpdate) {
            float delta = float(scoreChoice) - scoreWheelPosition;
            delta = std::remainder(delta, 32.f);
            scoreWheelPosition += std::clamp(delta, -.0625f, .0625f);
        }
        return;
    }
    if (mode != Mode::Menu) {
        if (frontPage == FrontPage::Scores && frontTicks >= 501)
            returnToTitle();
        return;
    }
    if (frontPage == FrontPage::Keys)
        return;
    if (frontPage == FrontPage::Demo) {
        if (!attractGame) {
            attractGame = std::make_shared<Game>(2000 + uint32_t(sourceFrame));
            attractGame->start(8 + int(sourceFrame % 16));
        }
        auto &demo = *attractGame;
        demo.fireHeld = true;
        const Enemy *target = nullptr;
        for (const auto &e : demo.enemies)
            if (!e.dead && hostile(e.kind) && (!target || e.depth < target->depth))
                target = &e;
        if (sourceUpdate && target && target->lane != demo.lane)
            demo.steer(float(demo.directionTo(demo.lane, target->lane)));
        if (target && target->depth < .15f)
            demo.jump();
        demo.tick(dt);
        demo.sounds.clear();
        if (demo.mode != Mode::GameOver && frontAge < 30)
            return;
        attractGame.reset();
        frontPage = FrontPage::Title;
        frontAge = 0;
        frontTicks = 0;
        return;
    }
    if (frontTicks < 501)
        return;
    frontAge = 0;
    frontTicks = 0;
    switch (frontPage) {
    case FrontPage::Title:
        frontPage = FrontPage::Scores;
        break;
    case FrontPage::Scores:
        frontPage = FrontPage::Credits;
        break;
    case FrontPage::Credits:
        frontPage = FrontPage::Tutorial;
        break;
    case FrontPage::Tutorial:
        frontPage = FrontPage::Demo;
        break;
    default:
        frontPage = FrontPage::Title;
        break;
    }
}
std::vector<std::string> Game::frontendLines() const {
    if (enteringScore) {
        std::string candidate = scoreChoice == 30 ? "DEL"
                                : scoreChoice == 31
                                    ? "END"
                                    : std::string(1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ .!-"[scoreChoice]);
        return {enteringBrag   ? "YOU ARE NUMBER ONE! / FIVE LETTER BRAG"
                : pendingScore ? "CONGRATULATIONS / HIGH SCORE"
                               : "YOU GOT A KEY!",
                scoreInitials, "SELECT " + candidate, "WHEEL SELECT / LMB ACCEPT / RMB DELETE",
                "ENTER FINISH / TYPE LETTERS ALSO WORKS"};
    }
    if (frontPage == FrontPage::Keys) {
        std::vector<std::string> lines{"PLAYER ACCESS KEYS", "WHEEL CHOOSE / ENTER OR LMB USE"};
        for (size_t i = 0; i < accessKeys.size(); ++i) {
            const auto &key = accessKeys[i];
            lines.push_back(std::string(int(i) == keyCursor ? "> " : "  ") +
                            (key.unlocked ? key.initials + " / WEB " + std::to_string(key.unlocked)
                                          : "--- / NO KEY"));
        }
        lines.push_back("ESC BACK / NEW GAMES START AT WEB 1");
        return lines;
    }
    if (frontPage == FrontPage::Scores) {
        std::vector<std::string> lines{"TEMPEST DUDES", "NUMBER ONE / " + brag};
        for (const auto &e : highScores)
            lines.push_back(e.initials + "  " + std::to_string(e.score));
        return lines;
    }
    if (frontPage == FrontPage::Congratulations)
        return beastly ? std::vector<std::string>{"TOTALLY AWESOME!",
                                                  "YOU BEAT THE GAME IN BEASTLY MODE!",
                                                  "YOU HAD BETTER HAVE ABOUT SIX CUPS OF TEA",
                                                  "WITH REFLEXES LIKE THAT, JOIN THE AIR FORCE",
                                                  "AND BECOME A FIGHTER PILOT?",
                                                  "OUTSTANDING TEMPEST, DUDE!",
                                                  "FIRE TO CONTINUE"}
                       : std::vector<std::string>{"YOU BEAT THE GAME!",
                                                  "NOW TRY TEMPEST - THE BEASTLY MODE!",
                                                  "O AT THE TITLE ENABLES THE HARDER GAME",
                                                  "ALL POINTS IN BEASTLY MODE ARE DOUBLED",
                                                  "HAVE A NICE CUP OF TEA TO CALM DOWN!",
                                                  "FIRE TO CONTINUE"};
    if (frontPage == FrontPage::Credits)
        return {"ORIGINAL GAME DESIGNED BY / DAVE THEURER",
                "JAGUAR VERSION BY / YAK",
                "BITMAP ARTWORK BY / JOBY",
                "TUNES BY / IMAGITEC DESIGN",
                "GAME TESTING / JOE, ANDREW, HANS AND GOKU",
                "VOICES / TED AND CARRIE TAHQUECHI",
                "PRODUCED BY / JOHN SKRUCH"};
    if (frontPage == FrontPage::Tutorial)
        return {"COLLECT POWERUPS!",
                "PARTICLE LASER / THEN JUMP / THEN A.I. DROID",
                "WHEEL ROTATES / HOLD LMB TO FIRE",
                "RMB SUPERZAPPER / MMB OR SPACE JUMP",
                "THREE BONUS TOKENS OPEN A BONUS COURSE",
                "BONUS WHEEL ADDS DRIFT / REVERSE BRAKES",
                "RINGS / LMB UP / RMB DOWN",
                "TUNNEL / STAY ON THE GREEN TRACK"};
    if (frontPage == FrontPage::Demo)
        return {"DEMONSTRATION", "ENTER OR LMB STARTS A NEW GAME"};
    return {"TEMPEST 2000", "ENTER / LMB START AT WEB 1", "K USE AN EARNED PLAYER KEY"};
}
} // namespace tempest
