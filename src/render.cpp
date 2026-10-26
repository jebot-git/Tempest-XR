#include "render.hpp"
#include "meshes.hpp"
#include <SDL_opengl.h>
#include <array>
#include <cstdio>
#include <map>

namespace tempest {
static constexpr Color cyan{.12f, .9f, 1}, pink{1, .12f, .52f}, lime{.65f, 1, .2f},
    amber{1, .65f, .2f}, white{.85f, .97f, 1};
static const Color palette[] = {pink, {.7f, .28f, 1}, amber, lime};
static Color enemyColor(Kind kind) {
    switch (kind) {
    case Kind::Flipper:
        return pink;
    case Kind::Tanker:
        return {.7f, .28f, 1};
    case Kind::Spiker:
        return amber;
    case Kind::Powerup:
        return lime;
    case Kind::Fuseball:
    case Kind::FuseTanker:
        return {1, .35f, .06f};
    case Kind::Pulsar:
    case Kind::PulsarTanker:
    case Kind::PulsarSpark:
        return {1, 1, .15f};
    case Kind::SuperFlipper:
        return {.15f, .45f, 1};
    case Kind::SuperFlipper2:
        return {1, .65f, .1f};
    case Kind::Mirror:
        return {.7f, .95f, 1};
    case Kind::Adroid:
        return {1, .1f, .1f};
    case Kind::Beast:
        return {1, .15f, .8f};
    case Kind::EnemyShot:
        return {1, .35f, .2f};
    }
    return cyan;
}
void Renderer::releaseGL() {
    for (auto &state : meltStates) {
        if (state.texture)
            glDeleteTextures(1, &state.texture);
        state = {};
    }
}
void Renderer::finishFrame(const Game &g, int view, int x, int y, int width, int height) {
    auto &state = meltStates.at(size_t(view));
    bool changed =
        state.seeded && state.previous != g.mode &&
        ((state.previous == Mode::Menu && g.mode == Mode::Playing) || g.mode == Mode::GameOver ||
         g.mode == Mode::Menu || g.mode == Mode::Bonus || g.mode == Mode::Victory);
    if (changed)
        state.started = g.time;
    bool resized = state.width != width || state.height != height;
    if (!state.texture) {
        glGenTextures(1, &state.texture);
        resized = true;
    }
    glBindTexture(GL_TEXTURE_2D, state.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    float age = g.time - state.started;
    if (state.seeded && !resized && age >= 0 && age < .65f) {
        // ffade's scale/rotate/intensity feedback, with an independent history per eye.
        float dt = std::clamp(g.time - state.lastTime, 0.f, .05f);
        float scale = std::pow(500.f / 512.f, dt * 60);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glRotatef(.7f * std::max(0.f, 1 - age / .12f) * dt * 60, 0, 0, 1);
        glScalef(scale, scale, 1);
        glColor4f(.97f, .97f, .97f, .88f * (1 - age / .65f));
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(-1, -1);
        glTexCoord2f(1, 0);
        glVertex2f(1, -1);
        glTexCoord2f(1, 1);
        glVertex2f(1, 1);
        glTexCoord2f(0, 1);
        glVertex2f(-1, 1);
        glEnd();
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_TEXTURE_2D);
        glDepthMask(GL_TRUE);
    }
    glBindTexture(GL_TEXTURE_2D, state.texture);
    if (resized)
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, width, height, 0);
    else
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);
    state.width = width;
    state.height = height;
    state.previous = g.mode;
    state.seeded = true;
    state.lastTime = g.time;
}
static Color spectrum(float hue, float alpha = 1) {
    constexpr float tau = 6.2831853f;
    return {.55f + .45f * std::cos(hue * tau), .55f + .45f * std::cos((hue - .3333f) * tau),
            .55f + .45f * std::cos((hue - .6667f) * tau), alpha};
}
static std::string number(int n, int digits = 0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%0*d", digits, n);
    return buf;
}
static const std::map<char, std::array<uint8_t, 7>> glyphs = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {31, 4, 4, 4, 4, 4, 31}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},       {'.', {0, 0, 0, 0, 0, 12, 12}},
    {'/', {1, 1, 2, 4, 8, 16, 16}},      {':', {0, 12, 12, 0, 12, 12, 0}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},       {'[', {14, 8, 8, 8, 8, 8, 14}},
    {']', {14, 2, 2, 2, 2, 2, 14}},      {'>', {16, 8, 4, 2, 4, 8, 16}},
    {'<', {1, 2, 4, 8, 4, 2, 1}},        {'=', {0, 0, 31, 0, 31, 0, 0}},
    {'%', {17, 2, 4, 4, 8, 16, 17}}};
void Renderer::rect(float x, float y, float w, float h, Color c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}
void Renderer::text(float x, float y, const std::string &value, float scale, Color color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    float origin = x;
    for (char ch : value) {
        if (ch == '\n') {
            x = origin;
            y += 10 * scale;
            continue;
        }
        auto found = glyphs.find(ch);
        if (found != glyphs.end())
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (found->second[row] & (1 << (4 - col))) {
                        float xx = x + float(col) * scale, yy = y + float(row) * scale,
                              s = scale * .87f;
                        glVertex2f(xx, yy);
                        glVertex2f(xx + s, yy);
                        glVertex2f(xx + s, yy + s);
                        glVertex2f(xx, yy + s);
                    }
        x += 6 * scale;
    }
    glEnd();
}
void Renderer::line(Vec3 a, Vec3 b, Color c) {
    lines.push_back({a, b, c});
}
void Renderer::flush() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    for (int pass = 0; pass < 3; pass++) {
        glLineWidth(pass == 0 ? 6.f : pass == 1 ? 3.f : 1.f);
        float alpha = pass == 0 ? .08f : pass == 1 ? .23f : .9f;
        glBegin(GL_LINES);
        for (auto &l : lines) {
            glColor4f(l.color.r, l.color.g, l.color.b, l.color.a * alpha);
            glVertex3f(l.a.x, l.a.y, l.a.z);
            glVertex3f(l.b.x, l.b.y, l.b.z);
        }
        glEnd();
    }
    glLineWidth(1);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    lines.clear();
}
void Renderer::enemy(const Game &g, const Enemy &e) {
    Vec3 p = g.enemyPosition(e);
    auto a = g.edge(e.lane, e.depth), b = g.edge(e.lane + 1, e.depth);
    Vec3 tangent = b - a;
    float length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
    tangent = tangent * (1 / std::max(.001f, length));
    Vec3 normal = {-tangent.y, tangent.x, 0};
    float s = e.kind == Kind::Tanker ? .13f : .095f;
    auto point = [&](float x, float y) { return p + tangent * (x * s) + normal * (y * s); };
    Color c = enemyColor(e.kind);
    const char *original = nullptr;
    switch (e.kind) {
    case Kind::Flipper:
        original = "s_flipper";
        break;
    case Kind::Tanker:
    case Kind::FuseTanker:
    case Kind::PulsarTanker:
        original = "s_fliptank";
        break;
    case Kind::SuperFlipper:
        original = "s_flip1";
        break;
    case Kind::SuperFlipper2:
        original = "s_flip2";
        break;
    case Kind::Mirror:
        original = "mirr";
        break;
    case Kind::Adroid:
        original = "adroid";
        break;
    case Kind::Beast:
        original = "hornm1";
        break;
    case Kind::EnemyShot:
        original = "pshot";
        break;
    default:
        break;
    }
    if (e.kind == Kind::Pulsar || e.kind == Kind::PulsarSpark) {
        static const char *frames[] = {"spuls6", "spuls6", "spuls6", "spuls5",
                                       "spuls4", "spuls3", "spuls2", "spuls1"};
        original = frames[int(g.waveTime * 8) % 8];
    }
    if (e.kind == Kind::Fuseball) {
        for (int arm = 0; arm < 4; arm++)
            mesh(arm % 2 ? "fbpiece1" : "fbpiece2", p, tangent, normal, s, c,
                 g.time * 5 + float(arm) * 1.5707963f);
        return;
    }
    if (original) {
        bool flip = e.kind == Kind::Flipper || e.kind == Kind::SuperFlipper ||
                    e.kind == Kind::SuperFlipper2;
        float fold = flip && e.flip < .2f ? std::cos(e.flip / .2f * 3.14159265f) : 1;
        mesh(original, p, tangent, normal, s, c,
             e.kind == Kind::Mirror || e.kind == Kind::Adroid || e.kind == Kind::EnemyShot
                 ? g.time * 3
                 : 0,
             fold);
        if (e.kind == Kind::PulsarTanker)
            mesh("spuls4", p + Vec3{0, 0, .005f}, tangent, normal, s * .55f, amber);
        if (e.kind == Kind::FuseTanker)
            mesh("fbpiece1", p + Vec3{0, 0, .005f}, tangent, normal, s * .6f, amber, g.time * 3);
        if (e.kind == Kind::Mirror || e.kind == Kind::SuperFlipper || e.kind == Kind::SuperFlipper2)
            for (int i = 0; i < e.hp; i++)
                line(point(-.6f + float(i) * .4f, -1.6f), point(-.6f + float(i) * .4f, -1.9f),
                     white);
        return;
    }
    if (e.kind != Kind::Powerup) {
        glColor4f(c.r, c.g, c.b, .3f);
        glBegin(GL_TRIANGLES);
        for (auto v : {point(-1.4f, -.8f), point(1.4f, .8f), point(-1.4f, .8f), point(-1.4f, -.8f),
                       point(1.4f, -.8f), point(1.4f, .8f)})
            glVertex3f(v.x, v.y, v.z);
        glEnd();
    }
    if (e.kind == Kind::Fuseball || e.kind == Kind::Beast) {
        int tips = e.kind == Kind::Beast ? 8 : 14;
        for (int i = 0; i < tips; i++) {
            float a = float(i) * 6.2831853f / float(tips) + g.time * 4,
                  b = float(i + 1) * 6.2831853f / float(tips) + g.time * 4;
            float r = i % 2 ? 1 : 1.8f, r2 = i % 2 ? 1.8f : 1;
            line(point(std::cos(a) * r, std::sin(a) * r), point(std::cos(b) * r2, std::sin(b) * r2),
                 c);
        }
        line(point(-.4f, 0), point(.4f, 0), white);
    } else if (e.kind == Kind::Pulsar || e.kind == Kind::PulsarSpark) {
        for (int i = 0; i < 6; i++)
            line(point(-1.5f + float(i) * .5f, i % 2 ? 1.2f : -1.2f),
                 point(-1.f + float(i) * .5f, i % 2 ? -1.2f : 1.2f), g.pulseActive() ? white : c);
    } else if (e.kind == Kind::Mirror || e.kind == Kind::Adroid) {
        float a = g.time * 2;
        auto q = [&](float x, float y) {
            return point(x * std::cos(a) - y * std::sin(a), x * std::sin(a) + y * std::cos(a));
        };
        for (int j = 0; j < 2; j++) {
            float r = j ? .6f : 1.5f;
            Vec3 v[] = {q(-r, -r), q(r, -r), q(r, r), q(-r, r)};
            for (int i = 0; i < 4; i++)
                line(v[i], v[(i + 1) % 4], c);
        }
        if (e.kind == Kind::Adroid) {
            line(point(0, 1.5f), point(0, 2.4f), c);
            line(point(-.4f, 0), point(.4f, 0), white);
        } else
            for (int i = 0; i < e.hp; i++)
                line(point(-1.2f + float(i) * .5f, -1.6f), point(-1.2f + float(i) * .5f, -1.9f),
                     white);
    } else if (e.kind == Kind::EnemyShot) {
        for (int i = 0; i < 4; i++) {
            float a = float(i) * 1.5707963f + g.time * 10, b = a + 1.5707963f;
            line(point(std::cos(a), std::sin(a)), point(std::cos(b), std::sin(b)), c);
        }
    } else if (e.kind == Kind::Flipper || e.kind == Kind::SuperFlipper ||
               e.kind == Kind::SuperFlipper2) {
        float f = std::cos(e.flip * 5);
        line(point(-1.4f, -.8f), point(1.4f, .8f * f), c);
        line(point(1.4f, .8f * f), point(-1.4f, .8f), c);
        line(point(-1.4f, .8f), point(1.4f, -.8f * f), c);
        line(point(1.4f, -.8f * f), point(-1.4f, -.8f), c);
        if (e.kind != Kind::Flipper) {
            line(point(-1.6f, 0), point(0, 1.4f), white);
            line(point(0, 1.4f), point(1.6f, 0), white);
            for (int i = 0; i < e.hp; i++)
                line(point(-.6f + float(i) * .6f, -1), point(-.6f + float(i) * .6f, -1.3f), c);
        }
    } else if (e.kind == Kind::Spiker) {
        for (int i = 0; i < 4; i++) {
            float t = float(i) * 1.5707963f;
            line(point(std::cos(t) * 1.5f, std::sin(t) * 1.5f),
                 point(std::cos(t + 1.5707963f) * 1.5f, std::sin(t + 1.5707963f) * 1.5f), c);
        }
        line(point(-1, 0), point(1, 0), white);
        line(point(0, -1), point(0, 1), white);
    } else {
        for (int i = 0; i < 4; i++) {
            float t = float(i) * 1.5707963f + g.time * (e.kind == Kind::Powerup ? 2.f : 0.f);
            line(point(std::cos(t) * 1.5f, std::sin(t) * 1.5f),
                 point(std::cos(t + 1.5707963f) * 1.5f, std::sin(t + 1.5707963f) * 1.5f), c);
        }
        line(point(-.6f, -.5f), point(.6f, .5f), c);
        line(point(.6f, -.5f), point(-.6f, .5f), c);
    }
}
bool Renderer::mesh(std::string_view name, Vec3 center, Vec3 tangent, Vec3 normal, float scale,
                    Color tint, float angle, float fold) {
    const auto *shape = sourceMesh(name);
    if (!shape)
        return false;
    auto point = [&](const MeshVertex &v) {
        float x = v.x * std::cos(angle) - v.y * std::sin(angle),
              y = (v.x * std::sin(angle) + v.y * std::cos(angle)) * fold;
        return center + tangent * (x * scale) + normal * (y * scale);
    };
    for (const auto &face : shape->faces) {
        Color accent = spectrum(float(face.color) / 256);
        glBegin(GL_POLYGON);
        for (const auto &v : face.vertices) {
            float light = .18f + .82f * v.light;
            glColor4f((tint.r * .8f + accent.r * .2f) * light,
                      (tint.g * .8f + accent.g * .2f) * light,
                      (tint.b * .8f + accent.b * .2f) * light, .95f);
            auto p = point(v);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
        for (size_t i = 0; i < face.vertices.size(); i++) {
            Color c = tint;
            c.a = .5f;
            line(point(face.vertices[i]), point(face.vertices[(i + 1) % face.vertices.size()]), c);
        }
    }
    return true;
}
void Renderer::grid(const Game &g, Vec3 offset, float alpha) {
    int n = g.web.lanes();
    Color theme = cyan;
    if (g.level > 1)
        theme = spectrum(.48f + float(g.level - 1) * .09f);
    for (int i = 0; i <= n; i++) {
        if (g.web.closed && i == n)
            break;
        for (int j = 0; j < 12; j++) {
            float d = float(j) / 12;
            Color c = theme;
            c.a = alpha * (.16f + .5f * (1 - d));
            line(g.edge(i, d) + offset, g.edge(i, float(j + 1) / 12) + offset, c);
        }
    }
    for (int j = 0; j <= 9; j++) {
        float d = float(j) / 9;
        Color c = theme;
        c.a = alpha * (j == 0 ? 1.f : .12f + (1 - d) * .12f);
        for (int i = 0; i < n; i++)
            line(g.edge(i, d) + offset, g.edge(i + 1, d) + offset, c);
    }
}
void Renderer::particleMessage(const Game &g) {
    if (g.messageTime <= 0 || g.mode == Mode::Menu)
        return;
    float t = 1 - g.messageTime / g.messageDuration;
    float melt = std::max(0.f, (t - .45f) / .55f);
    float pixel = .019f;
    Vec3 p = g.shipPosition() + Vec3{-float(g.message.size()) * 6 * pixel * .5f, .55f, 1.1f};
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(melt * 28, 0, 0, 1);
    glBegin(GL_QUADS);
    int letter = 0;
    for (char ch : g.message) {
        auto found = glyphs.find(ch);
        if (found != glyphs.end())
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (found->second[row] & (1 << (4 - col))) {
                        float seed = float(letter * 41 + row * 13 + col * 7);
                        float x =
                            float(letter * 6 + col) * pixel + std::sin(seed) * melt * melt * 1.5f;
                        float y = -float(row) * pixel + std::cos(seed * 1.7f) * melt * melt;
                        float z = std::sin(seed * .4f) * melt;
                        Color c = spectrum(float(letter) * .035f + g.time * .2f, 1 - melt);
                        glColor4f(c.r, c.g, c.b, c.a);
                        glVertex3f(x, y, z);
                        glVertex3f(x + pixel * .88f, y, z);
                        glVertex3f(x + pixel * .88f, y - pixel * .88f, z);
                        glVertex3f(x, y - pixel * .88f, z);
                    }
        letter++;
    }
    glEnd();

    glPopMatrix();
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::bonusWorld(const Game &g) {
    if (g.bonusType == 2) {
        glBegin(GL_QUADS);
        for (int z = 0; z < 32; z++)
            for (int x = -16; x < 16; x++) {
                Color c = spectrum(float(x + z) * .045f + g.bonusTime * .1f, .45f);
                if ((x + z) % 2)
                    c = {c.r * .2f, c.g * .2f, c.b * .2f, .4f};
                glColor4f(c.r, c.g, c.b, c.a);
                float zz = -3 - float(z) * 1.5f + std::fmod(g.bonusTime * 5, 1.5f),
                      xx = float(x) * 1.5f;
                for (auto p : {Vec3{xx, -2.2f, zz}, Vec3{xx + 1.5f, -2.2f, zz},
                               Vec3{xx + 1.5f, -2.2f, zz - 1.5f}, Vec3{xx, -2.2f, zz - 1.5f}})
                    glVertex3f(p.x, p.y, p.z);
            }
        glEnd();
    }
    float interval = g.bonusGateInterval();
    for (int k = g.bonusGate; k < std::min(g.bonusGate + 14, g.bonusGateCount()); k++) {
        Vec3 center = g.bonusTarget(k);
        float remaining = 2 + float(k) * interval - g.bonusTime;
        center.z = -3.8f - remaining * 6;
        float radius = g.bonusType == 1 ? .6f : .36f;
        Color c = spectrum(float(k) * .09f + g.time * .1f, .95f);
        for (int j = 0; j < 32; j++) {
            float a = float(j) * 6.2831853f / 32, b = float(j + 1) * 6.2831853f / 32;
            line(center + Vec3{std::cos(a) * radius, std::sin(a) * radius, 0},
                 center + Vec3{std::cos(b) * radius, std::sin(b) * radius, 0}, c);
            if (g.bonusType == 1 && k + 1 < g.bonusGateCount() && j % 4 == 0) {
                Vec3 next = g.bonusTarget(k + 1);
                next.z = center.z - interval * 6;
                line(center + Vec3{std::cos(a) * 2, std::sin(a) * 2, 0},
                     next + Vec3{std::cos(a) * 2, std::sin(a) * 2, 0},
                     spectrum(float(j) / 32 + g.time * .1f, .45f));
            }
        }
    }
    Vec3 cursor = g.bonusCursor() + Vec3{0, 0, -3.8f};
    line(cursor + Vec3{-.12f, 0, 0}, cursor + Vec3{.12f, 0, 0}, white);
    line(cursor + Vec3{0, -.12f, 0}, cursor + Vec3{0, .12f, 0}, white);
    for (auto &p : g.particles)
        line(p.pos, p.pos - p.velocity * .06f, spectrum(float(p.color) / 31, p.life / p.maxLife));
}
void Renderer::world(const Game &g, bool headset) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // Uniform full-sphere sky: every head direction sees stars, including behind/above.
    uint32_t rng = 42;
    auto random = [&]() {
        rng = rng * 1664525u + 1013904223u;
        return float(rng & 65535) / 65535.f;
    };
    glPointSize(1.4f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 2600; i++) {
        float y = random() * 2 - 1, angle = random() * 6.2831853f, radius = 100 + random() * 40;
        float r = std::sqrt(1 - y * y), x = r * std::cos(angle), z = r * std::sin(angle);
        Color c = spectrum(random(), .4f + random() * .6f);
        glColor4f(c.r, c.g, c.b, c.a);
        glVertex3f(x * radius, y * radius, z * radius);
    }
    glEnd();
    // Source ringstars/initpstarfield patterns, extended over the spherical backdrop.
    int skyMode = g.mode == Mode::Victory ? 2
                  : g.isBonus()           ? (g.bonusType == 2 ? 2 : 1)
                                          : (g.level - 1) / 8 % 3;
    glPointSize(2);
    glBegin(GL_POINTS);
    if (skyMode == 1) {
        for (int ring = 0; ring < 8; ring++)
            for (int j = 0; j < 64; j++) {
                float lat = (-.875f + float(ring) * .25f) * 1.5707963f,
                      angle = float(j) * 6.2831853f / 64 + g.time * .12f;
                lat += std::sin(angle * 3 + g.time * .3f) * .08f;
                float radius = 105 + std::sin(angle * 4 + float(ring)) * 6;
                Color c = spectrum(float(ring) / 8 + g.time * .02f);
                glColor4f(c.r, c.g, c.b, .9f);
                glVertex3f(std::cos(lat) * std::cos(angle) * radius, std::sin(lat) * radius,
                           std::cos(lat) * std::sin(angle) * radius);
            }
    } else if (skyMode == 2) {
        for (int j = 0; j < 768; j++) {
            float a = float(j) * .073f + g.time * .07f, b = float(j) * .051f - g.time * .05f;
            Vec3 d{std::sin(a), std::sin(b), std::cos(a * .63f + b * .37f)};
            float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            d = d * (115 / std::max(.1f, length));
            Color c = spectrum(float(j) / 128 + g.time * .03f);
            glColor4f(c.r, c.g, c.b, .85f);
            glVertex3f(d.x, d.y, d.z);
        }
    }
    glEnd();
    glPointSize(1.4f);
    glPushMatrix();
    if (headset || g.isBonus()) {
        auto camera = g.xrCameraPosition();
        glTranslatef(-camera.x, -camera.y, -camera.z);
    }
    if (g.isBonus()) {
        bonusWorld(g);
        flush();
        particleMessage(g);
        glPopMatrix();
        if (headset) {
            glPushMatrix();
            glTranslatef(-1.8f, 2.2f, -3.8f);
            glScalef(.007f, -.007f, .007f);
            text(0, 0,
                 g.bonusType == 1 ? "TUNNEL BONUS / WHEEL TO STEER"
                                  : "BONUS / WHEEL X / LMB UP / RMB DOWN",
                 1.5f, cyan);
            text(0, 24, "GATES " + number(g.bonusHits) + " / " + number(g.bonusGateCount()), 1.5f,
                 lime);
            glPopMatrix();
        }
        return;
    }
    int n = g.web.lanes();
    grid(g);
    if (headset && g.transitionDepth() > 0) {
        if (nextLevel != g.targetLevel) {
            nextWeb = std::make_unique<Game>();
            nextWeb->start(g.targetLevel);
            nextLevel = g.targetLevel;
        }
        Vec3 offset{0, 0, -Game::warpTravel};
        grid(*nextWeb, offset);
        // Join the exit of this web to the rim of the next web.
        for (int i = 0; i < n; i++) {
            int next = i * nextWeb->web.lanes() / n;
            line(g.edge(i, 1), nextWeb->edge(next, 0) + offset,
                 spectrum(float(i) / n + g.time * .2f, .7f));
        }
        for (int ring = 0; ring < 5; ring++) {
            float d = std::fmod(float(ring) * .2f + g.transitionDepth() * 2, 1.f);
            for (int i = 0; i < n; i++)
                line(g.edge(i, d), g.edge(i + 1, d), spectrum(d + g.time * .2f, .6f));
        }
    }
    // Highlight the current lane all the way to its vanishing edge.
    if (g.mode != Mode::Menu) {
        auto a = g.edge(g.lane, 0), b = g.edge(g.lane + 1, 0), c = g.edge(g.lane + 1, 1),
             d = g.edge(g.lane, 1);
        glColor4f(.1f, .8f, 1, .055f);
        glBegin(GL_QUADS);
        for (auto p : {a, b, c, d})
            glVertex3f(p.x, p.y, p.z + .005f);
        glEnd();
        Color active = cyan;
        active.a = .85f;
        line(a, d, active);
        line(b, c, active);
    }
    for (auto &s : g.shots) {
        auto p = g.position(float(s.lane), s.depth),
             q = g.position(float(s.lane), std::max(0.f, s.depth - .05f));
        line(p, q, white);
        if (g.boost) {
            for (int j = 0; j < 8; j++) {
                float a = float(j) * 6.2831853f / 8, b = float(j + 1) * 6.2831853f / 8;
                line(p + Vec3{std::cos(a) * .075f, std::sin(a) * .075f, 0},
                     p + Vec3{std::cos(b) * .075f, std::sin(b) * .075f, 0},
                     spectrum(float(j) / 8 + g.time));
            }
        }
    }
    for (auto &e : g.enemies)
        if (!e.dead) {
            enemy(g, e);
            if (((e.kind == Kind::Pulsar || e.kind == Kind::PulsarSpark) && g.pulseActive()) ||
                (e.kind == Kind::Adroid && e.rim && std::fmod(e.age, 2.f) > .9f)) {
                Color c = enemyColor(e.kind);
                c.a = .9f;
                line(g.edge(e.lane, 0), g.edge(e.lane, 1), c);
                line(g.edge(e.lane + 1, 0), g.edge(e.lane + 1, 1), c);
                for (int j = 0; j < 12; j++) {
                    float d = float(j) / 12;
                    Vec3 p = g.position(float(e.lane), d),
                         q = g.position(float(e.lane), float(j + 1) / 12);
                    p.x += j % 2 ? .035f : -.035f;
                    q.x += j % 2 ? -.035f : .035f;
                    line(p, q, c);
                }
            }
        }
    if (g.droidActive) {
        Vec3 p = g.position(g.droidLane, 0) + Vec3{0, 0, .25f};
        for (int j = 0; j < 2; j++) {
            float z = float(j) * .12f;
            Vec3 v[] = {p + Vec3{-.1f, -.1f, z}, p + Vec3{.1f, -.1f, z}, p + Vec3{.1f, .1f, z},
                        p + Vec3{-.1f, .1f, z}};
            for (int i = 0; i < 4; i++) {
                line(v[i], v[(i + 1) % 4], lime);
                if (!j)
                    line(v[i], v[i] + Vec3{0, 0, .12f}, cyan);
            }
        }
    }
    for (int i = 0; i < n; i++) {
        if (g.spikes[size_t(i)] < .99f) {
            line(g.position(float(i), g.spikes[size_t(i)]), g.position(float(i), 1), amber);
        }
    }
    if (g.mode == Mode::Menu) {
        for (int i = 0; i < 9; i++) {
            Enemy e{Kind(i % 3),
                    (i * 3 + 1) % n,
                    .12f + std::fmod(float(i) * .14f + g.time * .035f, .85f),
                    0,
                    g.time,
                    1};
            enemy(g, e);
        }
    }
    if (g.mode != Mode::GameOver && (g.invulnerable <= 0 || int(g.time * 12) % 2 == 0)) {
        Vec3 p = g.shipPosition(), a = g.edge(g.lane, 0), b = g.edge(g.lane + 1, 0), t = b - a;
        float length = std::sqrt(t.x * t.x + t.y * t.y);
        t = t * (1 / std::max(.001f, length));
        Vec3 normal = {-t.y, t.x, 0};
        p.z += .015f;
        std::string frame =
            (g.jumpTime > 0 ? "gsclaw" : "sclaw") + std::to_string(int(g.visualLane * 8) % 8);
        mesh(frame, p, t, normal, .16f, lime);
    }
    for (auto &p : g.particles) {
        Color c = p.color < 4 ? palette[p.color] : spectrum(float(p.color - 4) / 31 + g.time * .1f);
        c.a = p.life / p.maxLife;
        line(p.pos, p.pos - p.velocity * .07f, c);
        float s = .014f + .015f * c.a;
        line(p.pos + Vec3{-s, 0, 0}, p.pos + Vec3{s, 0, 0}, c);
        line(p.pos + Vec3{0, -s, 0}, p.pos + Vec3{0, s, 0}, c);
    }
    if (lastTrailTime != g.time) {
        if (g.jumpTime > 0 || g.boost || g.transitionDepth() > 0)
            shipTrail.push_back({g.shipPosition(), g.time});
        shipTrail.erase(std::remove_if(shipTrail.begin(), shipTrail.end(),
                                       [&](auto &p) { return g.time - p.second > .4f; }),
                        shipTrail.end());
        lastTrailTime = g.time;
    }
    for (size_t i = 1; i < shipTrail.size(); i++)
        line(shipTrail[i - 1].first, shipTrail[i].first,
             spectrum(float(i) * .08f + g.time, .6f * (1 - (g.time - shipTrail[i].second) / .4f)));
    if (g.zapTime > 0) {
        Vec3 origin = g.shipPosition();
        for (int i = 0; i < n; i++) {
            Vec3 end = g.position(float(i), .6f), previous = origin;
            for (int j = 1; j <= 8; j++) {
                float t = float(j) / 8;
                Vec3 p = origin * (1 - t) + end * t;
                if (j < 8) {
                    p.x += std::sin(float(i * 21 + j * 17) + std::floor(g.time * 45)) * .13f;
                    p.y += std::cos(float(i * 17 + j * 7) + std::floor(g.time * 45)) * .13f;
                }
                line(previous, p, spectrum(float(i) / n + g.time, g.zapTime));
                previous = p;
            }
        }
    }
    if (g.flash > 0) {
        for (int i = 0; i < n; i++) {
            Color c = white;
            c.a = g.flash;
            line(g.edge(i, 0), g.edge(i + 1, 1), c);
        }
    }
    flush();
    particleMessage(g);
    glPopMatrix();
    if (headset) {
        // Small world-space instrument panels sit above/below the rim.
        glDisable(GL_DEPTH_TEST);
        glPushMatrix();
        glTranslatef(-1.9f, 2.65f, -3.8f);
        glScalef(.008f, -.008f, .008f);
        text(0, 0, "SCORE " + number(g.score, 7), 2, white);
        text(0, 24, "WEB " + number(g.level, 2) + " / " + g.web.name, 1.2f, cyan);
        text(280, 0, "LIVES " + number(g.lives), 1.5f, lime);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(-1.9f, -2.45f, -3.8f);
        glScalef(.006f, -.006f, .006f);
        text(0, 0, "WHEEL MOVE / AUTO FIRE / RIGHT CLICK ZAP", 1.35f, cyan);
        if (g.mode != Mode::Playing) {
            std::string label = g.mode == Mode::Paused     ? "PAUSED - P TO RESUME"
                                : g.mode == Mode::Menu     ? "LEFT CLICK TO START"
                                : g.mode == Mode::GameOver ? "GAME OVER - CLICK TO RETRY"
                                                           : "AVOID SPIKES / MIDDLE CLICK JUMP";
            text(30, 25, label, 2, white);
        }
        glPopMatrix();
    }
}
void Renderer::desktop(const Game &g, int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(.006f, .009f, .025f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float top = .05f * std::tan(63.f * 3.14159265f / 360.f),
          aspect = float(width) / float(std::max(1, height));
    glFrustum(-top * aspect, top * aspect, -top, top, .05, 150);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (g.mode == Mode::Menu) {
        glTranslatef(1.6f, .25f, -2.6f);
        glRotatef(-10, 1, 0, 0);
        glRotatef(-12, 0, 1, 0);
    } else {
        glTranslatef(0, 0, -.55f);
    }
    world(g);
}
void Renderer::hud(const Game &g, int width, int height, const std::string &status,
                   float sensitivity, bool inverted, bool muted) {
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1440, 900, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    rect(0, 0, 1440, 108, {.015f, .025f, .055f, .92f});
    rect(48, 105, 1344, 1, {.2f, .5f, .65f, .3f});
    text(48, 26, "T / 2000", 3, cyan);
    text(48, 64, "VECTOR COMBAT SYSTEM", 1.4f, {.38f, .49f, .65f});
    text(460, 24, "SCORE", 1.4f, cyan);
    text(460, 49, number(g.score, 7), 3.5f, white);
    text(750, 24, "BEST", 1.4f, {.45f, .53f, .68f});
    text(750, 49, number(g.best, 7), 2.5f, {.6f, .68f, .8f});
    text(1040, 24, "WEB " + number(g.level, 2) + " / " + g.web.name, 1.5f, cyan);
    text(1040, 55, "LIVES " + number(std::max(0, g.lives)) + "  ZAP " + number(g.zaps), 2, lime);
    rect(0, 803, 1440, 97, {.015f, .025f, .055f, .95f});
    rect(48, 802, 1344, 1, {.2f, .5f, .65f, .3f});
    text(48, 829, "WHEEL", 1.6f, white);
    text(116, 829, "MOVE", 1.6f, {.4f, .54f, .68f});
    text(242, 829, "LMB", 1.6f, white);
    text(291, 829, g.autoFire ? "AUTO FIRE ON" : "HOLD TO FIRE", 1.6f, {.4f, .54f, .68f});
    text(511, 829, "RMB", 1.6f, pink);
    text(559, 829, "SUPERZAPPER", 1.6f, {.4f, .54f, .68f});
    text(771, 829, "MMB / SPACE", 1.6f, white);
    text(957, 829, "JUMP", 1.6f, {.4f, .54f, .68f});
    text(48, 869, "P PAUSE   F AUTO FIRE   V XR   B STEREO   F11 FULLSCREEN", 1.25f,
         {.35f, .46f, .61f});
    char settings[160];
    std::snprintf(settings, sizeof(settings), "[ / ] WHEEL %.2f  I %s  M %s", sensitivity,
                  inverted ? "REVERSED" : "NORMAL", muted ? "MUTED" : "AUDIO");
    text(823, 869, settings, 1.1f, {.35f, .46f, .61f});
    text(48, 123, status, 1.25f, {.35f, .56f, .65f});
    if (g.isBonus())
        text(48, 153,
             g.bonusType == 1 ? "TUNNEL BONUS / WHEEL TO STEER"
                              : "BONUS: WHEEL MOVES X / HOLD LEFT CLICK UP / RIGHT CLICK DOWN",
             1.5f, amber);
    if (g.mode == Mode::Menu) {
        rect(28, 171, 585, 584, {.006f, .012f, .032f, .9f});
        rect(48, 191, 4, 31, cyan);
        text(71, 198, "RECONSTRUCTION / 01", 1.8f, cyan);
        text(67, 263, "TEMPEST", 11, white);
        text(65, 364, "2000", 18, cyan);
        text(71, 531, "THE WEB IS WAITING.", 2, pink);
        text(71, 578, "SCROLL THE RIM. CLEAR THE TUBE.", 1.55f, {.57f, .67f, .8f});
        text(71, 608,
             "START WEB " + number(g.selectedLevel, 2) + " / UNLOCKED " +
                 number(g.unlockedLevel, 2),
             1.55f, cyan);
        text(71, 638, "WHEEL / ARROWS SELECT STARTING WEB", 1.25f, {.57f, .67f, .8f});
        rect(70, 662, 410, 62, {.07f, .73f, .86f, 1});
        text(100, 683, "> ENTER TO PLAY", 2.3f, {.015f, .04f, .065f});
        text(71, 743, "JEFF MINTER / ATARI 1994 - SOURCE STUDY", 1.05f, {.3f, .43f, .58f});
    } else {
        rect(48, 775, 1344, 3, {.08f, .16f, .23f, 1});
        rect(48, 775, 1344 * g.progress(), 3, cyan);
        if (g.mode == Mode::Paused || g.mode == Mode::GameOver || g.mode == Mode::Transition ||
            g.mode == Mode::Victory) {
            rect(375, 330, 690, 210, {.005f, .012f, .03f, .94f});
            rect(375, 330, 690, 2, cyan);
            std::string label = g.mode == Mode::Victory    ? "SUPERSTAR"
                                : g.mode == Mode::Paused   ? "PAUSED"
                                : g.mode == Mode::GameOver ? "GAME OVER"
                                                           : "WEB CLEAR";
            float x = 720 - float(label.size()) * 6 * 6 / 2;
            text(x, 375, label, 6, g.mode == Mode::GameOver ? pink : cyan);
            std::string sub = g.mode == Mode::Victory    ? "CAMPAIGN COMPLETE / ENTER TO PLAY AGAIN"
                              : g.mode == Mode::Paused   ? "P / ESC TO RESUME"
                              : g.mode == Mode::GameOver ? "ENTER TO PLAY AGAIN"
                                                         : "AVOID SPIKES / JUMP TO CLEAR";
            text(720 - float(sub.size()) * 6 * 1.8f / 2, 462, sub, 1.8f, white);
        }
        if (g.boost)
            text(1130, 751, "POWERED SHOTS", 1.5f, lime);
        if (!g.jumpEnabled)
            text(48, 751, "COLLECT POWERUPS TO UNLOCK JUMP", 1.25f, amber);
        else if (g.jumpCooldown > 0)
            text(48, 751, "JUMP RECHARGING", 1.5f, {.4f, .5f, .65f});
        else
            text(48, 751, "JUMP READY", 1.5f, lime);
        text(500, 751,
             "TOKENS " + number(g.bonusTokens) + "/3  POWERUP " +
                 number(std::min(6, g.powerupStage)) + "/6",
             1.4f, cyan);
    }
}
} // namespace tempest
