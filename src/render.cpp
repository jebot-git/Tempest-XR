#include "render.hpp"
#include "meshes.hpp"
#include "raster_data.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
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
    case Kind::MirrorShot:
    case Kind::BeastShot:
    case Kind::EnemyShot:
        return {1, .35f, .2f};
    }
    return cyan;
}
struct RasterAtlas {
    unsigned texture = 0;
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
static std::map<std::string, RasterAtlas> rasterAtlases;
static const RasterAtlas *rasterAtlas(std::string_view name) {
    auto [it, fresh] = rasterAtlases.try_emplace(std::string(name));
    auto &atlas = it->second;
    if (fresh) {
        std::filesystem::path directory = TEMPEST_ART_DIR;
        char *base = SDL_GetBasePath();
        if (base) {
            auto installed = std::filesystem::path(base) / "../share/tempest-xr/art";
            if (std::filesystem::is_directory(installed))
                directory = installed;
            SDL_free(base);
        }
        if (const char *overridePath = std::getenv("TEMPEST_ART_DIR"))
            directory = overridePath;
        SDL_Surface *raw = SDL_LoadBMP((directory / (std::string(name) + ".bmp")).c_str());
        if (raw) {
            SDL_Surface *surface = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(raw);
            if (surface) {
                atlas.width = surface->w;
                atlas.height = surface->h;
                atlas.rgba.resize(size_t(atlas.width * atlas.height) * 4);
                for (int y = 0; y < atlas.height; ++y)
                    std::copy_n(static_cast<uint8_t *>(surface->pixels) + y * surface->pitch,
                                atlas.width * 4, atlas.rgba.data() + size_t(y * atlas.width) * 4);
                SDL_FreeSurface(surface);
                glGenTextures(1, &atlas.texture);
                glBindTexture(GL_TEXTURE_2D, atlas.texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas.width, atlas.height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, atlas.rgba.data());
            }
        }
        if (!atlas.texture)
            std::cerr << "Original artwork unavailable: " << name << '\n';
    }
    return atlas.texture ? &atlas : nullptr;
}
bool Renderer::originalArtAvailable() {
    bool available = true;
    for (auto name : {"pic", "pic2", "pic4", "pic5", "pic6"})
        available = rasterAtlas(name) && available;
    return available;
}
static void rasterVertices(const RasterAtlas &atlas, const RasterRect &r, float x, float y, float w,
                           float h) {
    float u = float(r.x) / atlas.width, v = float(r.y) / atlas.height;
    float u2 = float(r.x + r.width) / atlas.width, v2 = float(r.y + r.height) / atlas.height;
    glTexCoord2f(u, v);
    glVertex2f(x, y);
    glTexCoord2f(u2, v);
    glVertex2f(x + w, y);
    glTexCoord2f(u2, v2);
    glVertex2f(x + w, y + h);
    glTexCoord2f(u, v2);
    glVertex2f(x, y + h);
}
static bool drawRaster(std::string_view name, float x, float y, float w, float h,
                       Color color = {1, 1, 1, 1}) {
    const auto *rect = sourceSprite(name);
    const auto *atlas = rect ? rasterAtlas(rect->atlas) : nullptr;
    if (!atlas)
        return false;
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    rasterVertices(*atlas, *rect, x, y, w, h);
    glEnd();
    glPopAttrib();
    return true;
}
static void fallbackGlyph(char ch, float x, float y, float scale);
static bool sourceText(int fontIndex, float x, float y, const std::string &value, float scale,
                       Color color) {
    const auto &font = sourceFonts[size_t(fontIndex)];
    const auto *atlas = rasterAtlas(font.atlas);
    if (!atlas)
        return false;
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glColor4f(color.r, color.g, color.b, color.a);
    float origin = x;
    std::vector<std::pair<char, std::array<float, 2>>> fallback;
    glBegin(GL_QUADS);
    for (unsigned char ch : value) {
        if (ch == '\n') {
            x = origin;
            y += 10 * scale;
            continue;
        }
        if (std::string_view("/+[]><=%").find(char(ch)) != std::string_view::npos)
            fallback.push_back({char(ch), {x, y}});
        else if (ch >= 32 && ch < 128 && ch != ' ') {
            auto xy = font.glyphs[size_t(ch - 32)];
            RasterRect rect{"",
                            font.atlas,
                            xy[0],
                            xy[1],
                            std::min(font.width, atlas->width - xy[0]),
                            std::min(font.height, atlas->height - xy[1])};
            rasterVertices(*atlas, rect, x, y, 5 * scale, 7 * scale);
        }
        x += 6 * scale;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (const auto &[ch, xy] : fallback)
        fallbackGlyph(ch, xy[0], xy[1], scale);
    glEnd();
    glPopAttrib();
    return true;
}
void Renderer::releaseGL() {
    for (auto &[name, atlas] : rasterAtlases)
        if (atlas.texture)
            glDeleteTextures(1, &atlas.texture);
    rasterAtlases.clear();
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
static void fallbackGlyph(char ch, float x, float y, float scale) {
    auto found = glyphs.find(ch);
    if (found == glyphs.end())
        return;
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
            if (found->second[row] & (1 << (4 - col))) {
                float xx = x + col * scale, yy = y + row * scale, size = scale * .87f;
                glVertex2f(xx, yy);
                glVertex2f(xx + size, yy);
                glVertex2f(xx + size, yy + size);
                glVertex2f(xx, yy + size);
            }
}
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
    if (sourceText(scale >= 3 ? 0 : 2, x, y, value, scale, color))
        return;
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
    if (e.entering) {
        line(p, p + Vec3{.012f, 0, 0}, enemyColor(e.kind));
        return;
    }
    Vec3 tangent = g.enemyTangent(e);
    Vec3 normal = {-tangent.y, tangent.x, 0};
    float s = e.kind == Kind::Tanker ? .13f : .095f;
    if (e.kind == Kind::Powerup && e.pickupFrames > 0)
        s *= 1 + float(e.pickupFrames) / 12;
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
        original = int(e.age * 5) % 2 ? "s_fliptank2" : "s_fliptank";
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
        mesh("hornm1", p, tangent, normal, s, c);
        if (e.hp >= 2)
            mesh("hornm2", p, tangent, normal, s, c);
        if (e.hp >= 3)
            mesh("hornm3", p, tangent, normal, s, c);
        return;
    case Kind::MirrorShot: {
        // ringbull's original 15x15 raster, repeated in Z by the Jaguar rex effect.
        const auto *atlas = rasterAtlas("pic2");
        if (atlas) {
            glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindTexture(GL_TEXTURE_2D, atlas->texture);
            glBegin(GL_QUADS);
            for (int trail = 2; trail >= 0; --trail) {
                glColor4f(1, 1, 1, 1 - float(trail) * .3f);
                for (auto xy : {std::pair{0, 0}, {1, 0}, {1, 1}, {0, 1}}) {
                    glTexCoord2f(float(215 + xy.first * 15) / atlas->width,
                                 float(115 + xy.second * 15) / atlas->height);
                    Vec3 v = point(float(xy.first * 3) - 1.5f, 1.5f - float(xy.second * 3));
                    glVertex3f(v.x, v.y, v.z - float(trail) * .06f);
                }
            }
            glEnd();
            glPopAttrib();
        }
        return;
    }
    case Kind::BeastShot:
        original = e.hp == 2 ? "hornm2" : "hornm3";
        break;
    case Kind::EnemyShot:
        original = "pshot";
        break;
    default:
        break;
    }
    if (e.kind == Kind::Pulsar || e.kind == Kind::PulsarSpark) {
        static const char *frames[] = {"spuls6", "spuls6", "spuls6", "spuls5", "spuls4", "spuls3",
                                       "spuls2", "spuls1", "spuls1", "spuls2", "spuls3", "spuls4",
                                       "spuls5", "spuls6", "spuls6", "spuls6"};
        original = frames[g.pulsePhase()];
    }
    if (e.kind == Kind::Fuseball) {
        for (int arm = 0; arm < 4; arm++)
            mesh(arm % 2 ? "fbpiece1" : "fbpiece2", p, tangent, normal, s, c,
                 g.time * 5 + float(arm) * 1.5707963f);
        return;
    }
    if (original) {
        mesh(original, p, tangent, normal, s, c,
             e.kind == Kind::Mirror || e.kind == Kind::Adroid || e.kind == Kind::EnemyShot
                 ? g.time * 3
                 : 0,
             1);
        if (e.kind == Kind::PulsarTanker)
            mesh("spuls4", p + Vec3{0, 0, .005f}, tangent, normal, s * .55f, amber);
        if (e.kind == Kind::FuseTanker)
            mesh("fbpiece1", p + Vec3{0, 0, .005f}, tangent, normal, s * .6f, amber, g.time * 3);
        if (e.kind == Kind::Mirror)
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
        float f = 1;
        line(point(-1.4f, -.8f), point(1.4f, .8f * f), c);
        line(point(1.4f, .8f * f), point(-1.4f, .8f), c);
        line(point(-1.4f, .8f), point(1.4f, -.8f * f), c);
        line(point(1.4f, -.8f * f), point(-1.4f, -.8f), c);
        if (e.kind != Kind::Flipper) {
            line(point(-1.6f, 0), point(0, 1.4f), white);
            line(point(0, 1.4f), point(1.6f, 0), white);
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
void Renderer::deathWorld(const Game &g) {
    if (g.deathTime <= 0)
        return;
    float t = g.deathProgress();
    Vec3 origin = g.deathOrigin();
    if (g.snatched) {
        Vec3 p = origin + Vec3{0, 0, -9 * t};
        mesh("sclaw0", p, {1, 0, 0}, {0, 1, 0}, .16f * (1 - .6f * t), lime, t * 6.2831853f);
        mesh("s_flipper", p + Vec3{0, .14f, 0}, {1, 0, 0}, {0, 1, 0}, .13f, pink, t * 6.2831853f);
        line(origin, p, spectrum(t, (1 - t) * .5f));
        return;
    }
    const auto *shape = sourceMesh("sclaw0");
    if (!shape)
        return;
    for (size_t i = 0; i < shape->faces.size(); ++i) {
        const auto &face = shape->faces[i];
        float angle = float(i) * 2.4f, spin = t * (5 + float(i));
        Vec3 p = origin + Vec3{std::cos(angle) * t * 1.4f, std::sin(angle) * t * 1.4f, -t * 2};
        Color c = spectrum(float(i) / shape->faces.size());
        glBegin(GL_POLYGON);
        for (auto v : face.vertices) {
            glColor4f(c.r * v.light, c.g * v.light, c.b * v.light, 1 - t);
            float x = (v.x * std::cos(spin) - v.y * std::sin(spin)) * .16f;
            float y = (v.x * std::sin(spin) + v.y * std::cos(spin)) * .16f;
            glVertex3f(p.x + x, p.y + y, p.z);
        }
        glEnd();
    }
}
void Renderer::grid(const Game &g, Vec3 offset, float alpha) {
    int n = g.web.lanes();
    Color theme = cyan;
    // The graphic outlives the 50-frame web-color holiday.
    if (g.oneUpTime > 101.f / 60)
        theme = spectrum(std::floor(g.time * 15) * .08f);
    else if (g.level > 1)
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
void Renderer::particleMessage(const Game &g, bool headset) {
    if (g.isBonus() && (g.message == "FOLLOW THE RINGS" || g.message == "TUNNEL BONUS"))
        return; // The bonus briefing already names the objective; keep its controls unobscured.
    if ((g.messageTime <= 0 && g.mode != Mode::GameOver) || g.mode == Mode::Menu)
        return;
    float t = g.mode == Mode::GameOver ? std::fmod(g.time, 3.f) / 3
                                       : 1 - g.messageTime / g.messageDuration;
    float melt = std::max(0.f, (t - .45f) / .55f);
    std::string sprite =
        g.mode == Mode::GameOver                                           ? "gameover"
        : g.message == "EXCELLENT"                                         ? "points2000"
        : g.message == "YES YES YES" || g.message.find("BONUS TOKEN") == 0 ? "excellent"
        : g.message == "250" || g.message == "500" || g.message == "750"   ? g.message
                                                                           : "";
    const auto *rect = sourceSprite(sprite);
    const auto &font = sourceFonts[0];
    const auto *atlas = rasterAtlas(rect ? rect->atlas : font.atlas);
    if (!atlas)
        return;
    float pixel = rect ? .009f : .0075f;
    float width = rect ? rect->width * pixel : float(g.message.size() * (font.width + 1)) * pixel;
    Vec3 origin =
        (headset || g.isBonus() || g.isBonusReturn() ? g.shipPosition() : Vec3{0, 0, -3.8f}) +
        Vec3{-width * .5f, .6f, 1.1f};
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    auto emit = [&](int ax, int ay, float x, float y, float seed) {
        if (ax < 0 || ay < 0 || ax >= atlas->width || ay >= atlas->height)
            return;
        const auto *rgba = atlas->rgba.data() + size_t(ay * atlas->width + ax) * 4;
        if (!rgba[3])
            return;
        float xx = origin.x + x * pixel + std::sin(seed) * melt * melt * 1.5f;
        float yy = origin.y - y * pixel + std::cos(seed * 1.7f) * melt * melt;
        float zz = origin.z + std::sin(seed * .4f) * melt;
        glColor4f(rgba[0] / 255.f, rgba[1] / 255.f, rgba[2] / 255.f, 1 - melt);
        float s = pixel * .95f;
        glVertex3f(xx, yy, zz);
        glVertex3f(xx + s, yy, zz);
        glVertex3f(xx + s, yy - s, zz);
        glVertex3f(xx, yy - s, zz);
    };
    if (rect) {
        for (int y = 0; y < rect->height; ++y)
            for (int x = 0; x < rect->width; ++x)
                emit(rect->x + x, rect->y + y, float(x), float(y), float(y * rect->width + x));
    } else {
        for (size_t i = 0; i < g.message.size(); ++i) {
            unsigned char ch = g.message[i];
            if (ch < 33 || ch >= 128)
                continue;
            auto xy = font.glyphs[size_t(ch - 32)];
            for (int y = 0; y < font.height; ++y)
                for (int x = 0; x < font.width; ++x)
                    emit(xy[0] + x, xy[1] + y, float(i * (font.width + 1) + x), float(y),
                         float(i * 997 + y * 31 + x));
        }
    }
    glEnd();
    glPopAttrib();
}
void Renderer::tunnelWorld(const Game &g) {
    constexpr float tau = 6.283185307f;
    float collapse =
        g.bonusFinished && !g.bonusWon ? std::max(0.f, 1 - g.tunnelResultTime / (33.f / 60)) : 1;
    auto point = [&](float angle, float ring) {
        return Vec3{std::cos(angle) * 1.4f * collapse, std::sin(angle) * 1.4f * collapse,
                    -3.8f - ring * .42f};
    };
    auto track = [&](int ring) {
        float a = g.tunnelTrackAngle(ring), b = g.tunnelTrackAngle(std::min(55, ring + 1));
        return a + std::remainder(b - a, tau) * g.tunnelInterpolation();
    };
    for (int ring = 0; ring < 55; ++ring) {
        float a = track(ring), b = track(ring + 1);
        glColor4f(.12f, .8f, .08f, .12f * collapse);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 16; ++i) {
            float offset = -tau / 8 + float(i) * tau / 64;
            for (auto p : {point(a + offset, float(ring)), point(b + offset, float(ring + 1))})
                glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
        Color green = ring == 0 && g.tunnelOnTrack() ? white : lime;
        green.a = (1 - float(ring) / 70) * collapse;
        for (float edge : {-tau / 8, tau / 8})
            line(point(a + edge, float(ring)), point(b + edge, float(ring + 1)), green);
        for (int i = 0; i < 16; ++i)
            line(point(a - tau / 8 + float(i) * tau / 64, float(ring)),
                 point(a - tau / 8 + float(i + 1) * tau / 64, float(ring)), green);
        const auto &seg = g.tunnel[size_t((g.tunnelHead + 8 + ring) & 63)];
        for (const auto &band : seg.bands) {
            Color c = spectrum(float(band.color) / 256, .65f * collapse * (1 - float(ring) / 70));
            for (int pixel = 0; pixel < band.pixels; ++pixel) {
                float angle = (float(band.phase) / 256 + float(pixel) / band.pixels) * tau;
                line(point(angle, float(ring)), point(angle + .015f, float(ring)), c);
            }
        }
    }
    int length = std::min(int(g.tunnelTrailAngles.size()), int(g.tunnelTrail) + 1);
    Vec3 previous{};
    for (int i = 0; i < length; ++i) {
        float angle = g.tunnelTrailAngles[size_t(i)];
        Vec3 p{std::cos(angle) * 1.4f, std::sin(angle) * 1.4f, -3.8f + float(i) * 9 / 160};
        Color c = spectrum(float(i) * .04f + g.time * .1f, collapse);
        float r = (i == 0 ? .09f : .045f) * collapse;
        Vec3 vertices[] = {p + Vec3{0, r, 0}, p + Vec3{r, 0, 0}, p + Vec3{0, -r, 0},
                           p + Vec3{-r, 0, 0}};
        for (int k = 0; k < 4; ++k) {
            line(vertices[k], vertices[(k + 1) % 4], c);
            line(vertices[k], p + Vec3{0, 0, r}, c);
            line(vertices[k], p + Vec3{0, 0, -r}, c);
        }
        if (i)
            line(previous, p, lime);
        previous = p;
    }
    Vec3 cursor = g.bonusCursor() + Vec3{0, 0, -3.8f};
    line(cursor + Vec3{-.12f, 0, 0}, cursor + Vec3{.12f, 0, 0}, white);
    line(cursor + Vec3{0, -.12f, 0}, cursor + Vec3{0, .12f, 0}, white);
}
void Renderer::bonusWorld(const Game &g) {
    if (g.bonusFinished && g.bonusWon && g.bonusType != 1) {
        Vec3 center = g.bonusCursor() + Vec3{0, 0, -7};
        float frame = std::floor(g.bonusTime * 60);
        float angle = (std::abs(float(int(frame / 4) & 127) - 63) - 31) * 6.2831853f / 256;
        for (int echo = 12; echo >= 0; --echo) {
            float a = angle - echo * .035f, size = .6f + echo * .12f;
            Vec3 tip = center + Vec3{std::sin(a) * size, std::cos(a) * size, -.8f};
            for (int side = 0; side < 4; ++side) {
                float b = a + side * 1.5707963f, c = b + 1.5707963f;
                Vec3 p = center + Vec3{std::cos(b) * size, std::sin(b) * size, .4f};
                Vec3 q = center + Vec3{std::cos(c) * size, std::sin(c) * size, .4f};
                Color color = spectrum(frame / 256 + side * .1f, 1.f / (1 + echo * .3f));
                line(p, q, color);
                line(p, tip, color);
            }
        }
        flush();
        glPushMatrix();
        glTranslatef(center.x, center.y, center.z + 1);
        drawRaster("excellent", -.6f, .25f, 1.2f, -.5f);
        glPopMatrix();
        return;
    }
    if (g.bonusType == 1) {
        tunnelWorld(g);
        return;
    }
    if (g.bonusType != 1) {
        const auto *ground = rasterAtlas("pic4");
        glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT);
        if (ground) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, ground->texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
        glBegin(GL_QUADS);
        for (int z = 0; z < 32; z++)
            for (int x = -16; x < 16; x++) {
                Color c = g.bonusType == 2
                              ? spectrum(float(x + z) * .025f + g.bonusTime * .04f, .65f)
                              : Color{1, 1, 1, .6f};
                glColor4f(c.r, c.g, c.b, c.a);
                float zz = -3 - float(z) * 1.5f + std::fmod(g.bonusDistance / 32, 1.5f),
                      xx = float(x) * 1.5f;
                float u = float(x) / 8, v = float(z) / 8 - g.bonusDistance / 512;
                glTexCoord2f(u, v);
                glVertex3f(xx, -2.2f, zz);
                glTexCoord2f(u + .125f, v);
                glVertex3f(xx + 1.5f, -2.2f, zz);
                glTexCoord2f(u + .125f, v + .125f);
                glVertex3f(xx + 1.5f, -2.2f, zz - 1.5f);
                glTexCoord2f(u, v + .125f);
                glVertex3f(xx, -2.2f, zz - 1.5f);
            }
        glEnd();
        glPopAttrib();
    }
    for (int k = g.bonusGate; k < std::min(g.bonusGate + 14, g.bonusGateCount()); k++) {
        Vec3 center = g.bonusTarget(k);
        center.z = g.bonusGateZ(k);
        float radius = g.bonusType == 1 ? .6f : .25f;
        Color c = spectrum(float(k) * .09f + g.time * .1f, .95f);
        for (int j = 0; j < 32; j++) {
            float a = float(j) * 6.2831853f / 32, b = float(j + 1) * 6.2831853f / 32;
            line(center + Vec3{std::cos(a) * radius, std::sin(a) * radius, 0},
                 center + Vec3{std::cos(b) * radius, std::sin(b) * radius, 0}, c);
            if (g.bonusType == 1 && k + 1 < g.bonusGateCount() && j % 4 == 0) {
                Vec3 next = g.bonusTarget(k + 1);
                next.z = g.bonusGateZ(k + 1);
                line(center + Vec3{std::cos(a) * 2, std::sin(a) * 2, 0},
                     next + Vec3{std::cos(a) * 2, std::sin(a) * 2, 0},
                     spectrum(float(j) / 32 + g.time * .1f, .45f));
            }
        }
        // gatefx icons: upward/downward impulse, speed boost and victory.
        int effect = g.bonusGateEffect(k);
        if (effect == 1 || effect == 2) {
            float sign = effect == 1 ? 1.f : -1.f;
            line(center + Vec3{0, -.12f * sign, 0}, center + Vec3{0, .12f * sign, 0}, white);
            line(center + Vec3{-.07f, .04f * sign, 0}, center + Vec3{0, .12f * sign, 0}, white);
            line(center + Vec3{.07f, .04f * sign, 0}, center + Vec3{0, .12f * sign, 0}, white);
        } else if (effect == 3) {
            Vec3 a = center + Vec3{-.06f, .14f, 0}, b = center + Vec3{.05f, .02f, 0},
                 c = center + Vec3{-.03f, -.02f, 0}, d = center + Vec3{.06f, -.14f, 0};
            line(a, b, amber);
            line(b, c, amber);
            line(c, d, amber);
        } else if (effect == 4) {
            auto tip = center + Vec3{0, .13f, 0};
            line(center + Vec3{-.12f, -.1f, 0}, tip, effect == 3 ? amber : lime);
            line(center + Vec3{.12f, -.1f, 0}, tip, effect == 3 ? amber : lime);
            line(center + Vec3{-.12f, -.1f, 0}, center + Vec3{.12f, -.1f, 0}, white);
        }
    }
    Vec3 cursor = g.bonusCursor() + Vec3{0, 0, -3.8f};
    line(cursor + Vec3{-.12f, 0, 0}, cursor + Vec3{.12f, 0, 0}, white);
    line(cursor + Vec3{0, -.12f, 0}, cursor + Vec3{0, .12f, 0}, white);
    for (auto &p : g.particles)
        line(p.pos, p.pos - p.velocity * .06f, spectrum(float(p.color) / 31, p.life / p.maxLife));
}
void Renderer::effectsWorld(const Game &g) {
    constexpr float tau = 6.2831853f;
    for (const auto &fx : g.effects) {
        Color color = spectrum(float(fx.color) / 256, 1.f);
        if (fx.kind == EffectKind::Score) {
            flush();
            std::string sprite = std::to_string(fx.color);
            if (fx.color == 1)
                continue;
            glPushMatrix();
            glTranslatef(fx.pos.x, fx.pos.y + float(fx.frame) * .005f, fx.pos.z);
            float size = .2f + float(fx.frame) * .006f;
            drawRaster(sprite, -size, size * .3f, size * 2, -size * .6f,
                       {1, 1, 1, 1 - float(fx.frame) / 32});
            glPopMatrix();
            continue;
        }
        if (fx.kind == EffectKind::Splatter || fx.kind == EffectKind::Pickup) {
            float rotation =
                float(fx.frame * (fx.kind == EffectKind::Splatter ? 3 : 5)) * tau / 256;
            float radius = fx.kind == EffectKind::Splatter ? .06f + fx.frame * .003f : .12f;
            for (int i = 0; i < 4; ++i) {
                float a = rotation + i * tau / 4;
                Vec3 p = fx.pos + Vec3{std::cos(a) * radius, std::sin(a) * radius, 0};
                Color c = fx.kind == EffectKind::Pickup ? spectrum(float(fx.frame) / 16) : amber;
                line(fx.pos, p, c);
            }
            continue;
        }
        if (fx.kind == EffectKind::Firework && fx.frame <= fx.duration) {
            line(fx.pos, fx.pos - fx.velocity * .04f, color);
            continue;
        }
        bool sphere = fx.kind == EffectKind::Sphere || fx.kind == EffectKind::Firework;
        int age = fx.kind == EffectKind::Firework ? fx.frame - fx.duration - 1 : fx.frame;
        float radius = age * (sphere ? .025f : .015f);
        color.a = std::clamp(
            1 - float(age) / (fx.kind == EffectKind::Firework ? 63 : fx.duration + 1), 0.f, 1.f);
        int rings = sphere ? (fx.kind == EffectKind::Firework ? 32 : 64) : 3;
        for (int ring = 0; ring < rings; ++ring) {
            int pixels = sphere ? (fx.kind == EffectKind::Firework ? 8 : 16) : 16 - ring * 2;
            float latitude = sphere ? -1.5707963f + (ring + .5f) * 3.14159265f / rings : 0;
            float r = sphere ? radius * std::cos(latitude) : radius * float(1 << ring);
            for (int pixel = 0; pixel < pixels; ++pixel) {
                float phase = (float(pixel) / pixels + ring * .03125f) * tau + g.time;
                Vec3 p = fx.pos + Vec3{std::cos(phase) * r, std::sin(phase) * r,
                                       sphere ? std::sin(latitude) * radius : -ring * .225f};
                line(p, p + Vec3{.012f, .005f, 0}, color);
            }
        }
    }
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
    if (g.transitionDepth() > 0 || g.isBonus()) {
        float speed = g.isBonus() ? g.bonusSpeed / 30 : 1 + g.warpDistance();
        for (int i = 0; i < 256; ++i) {
            float angle = float(i) * 2.399963f, y = 1 - 2 * (float(i) + .5f) / 256;
            float r = std::sqrt(1 - y * y);
            Vec3 p{r * std::cos(angle) * 110, y * 110, r * std::sin(angle) * 110};
            line(p, p + Vec3{0, 0, std::min(18.f, speed * 1.2f)}, spectrum(float(i) / 256, .55f));
        }
        flush();
    }
    glPushMatrix();
    if (headset || g.isBonus() || g.isBonusReturn()) {
        auto camera = g.xrCameraPosition();
        glTranslatef(-camera.x, -camera.y, -camera.z);
    }
    effectsWorld(g);
    if (g.isBonus()) {
        bonusWorld(g);
        flush();
        particleMessage(g, headset);
        glPopMatrix();
        if (headset)
            headsetHud(g);
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
                (e.kind == Kind::Adroid && e.droidMotion == DroidMotion::Zapping)) {
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
        if (g.hasSpike(i)) {
            line(g.position(float(i), g.spikes[size_t(i)]), g.position(float(i), 1),
                 g.superSpikes[size_t(i)] ? spectrum(g.time * 3) : amber);
        }
    }
    if (g.mode == Mode::Menu) {
        for (int i = 0; i < 9; i++) {
            Enemy e{Kind(i % 3), (i * 3 + 1) % n,
                    .12f + std::fmod(float(i) * .14f + g.time * .035f, .85f), g.time};
            enemy(g, e);
        }
    }
    deathWorld(g);
    if (g.deathTime <= 0 && g.mode != Mode::GameOver &&
        (g.invulnerable <= 0 || int(g.time * 12) % 2 == 0)) {
        Vec3 p = g.shipPosition(), a = g.edge(g.lane, 0), b = g.edge(g.lane + 1, 0), t = b - a;
        float length = std::sqrt(t.x * t.x + t.y * t.y);
        t = t * (1 / std::max(.001f, length));
        Vec3 normal = {-t.y, t.x, 0};
        p.z += .015f;
        int animation = int(g.visualLane * 8) % 8;
        if (g.jumpTime > 0) {
            int phase = std::clamp(int((.85f - g.jumpTime) / .85f * 16), 0, 15);
            animation = phase < 8 ? phase : 15 - phase;
        }
        std::string frame = (g.jumpTime > 0 ? "gsclaw" : "sclaw") + std::to_string(animation);
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
    if (headset || g.mode != Mode::GameOver)
        particleMessage(g, headset);
    if (g.oneUpTime > 0) {
        Vec3 p = g.shipPosition() + Vec3{0, .8f + (151.f / 60 - g.oneUpTime) * .1f, 1};
        glPushMatrix();
        glTranslatef(p.x, p.y, p.z);
        drawRaster("oneup", -.3f, .1f, .6f, -.2f, {1, 1, 1, std::min(1.f, g.oneUpTime)});
        glPopMatrix();
    }
    glPopMatrix();
    if (headset && g.mode == Mode::Menu) {
        glPushMatrix();
        glTranslatef(0, .8f, -3.8f);
        drawRaster("logo", -1.5f, .5625f, 3, -1.125f);
        glPopMatrix();
    }
    if (headset)
        headsetHud(g);
}
std::vector<std::string> Renderer::xrHudLines(const Game &g) {
    std::vector<std::string> result;
    result.push_back("SCORE " + number(g.score, 7) + "   LIVES " + number(std::max(0, g.lives)));
    result.push_back("WEB " + number(g.level, 2) + " / " + g.web.name + "   ZAP " + number(g.zaps));
    std::string jump = "JUMP LOCKED / COLLECT POWERUPS";
    if (g.jumpEnabled) {
        if (g.jumpCooldown > 0) {
            char timer[64];
            std::snprintf(timer, sizeof(timer), "JUMP RECHARGING %.1fS", g.jumpCooldown);
            jump = timer;
        } else
            jump = "JUMP READY / MIDDLE CLICK OR SPACE";
    }
    result.push_back(jump);
    result.push_back("TOKENS " + number(g.bonusTokens) + "/3   POWERUP " +
                     number(std::min(6, g.powerupStage)) + "/6");
    result.push_back(std::string("LASER ") + (g.boost ? "ON" : "OFF") + "   DROID " +
                     (g.droidActive ? "ON" : "OFF"));
    if (g.mode == Mode::Menu) {
        result.push_back("START WEB " + number(g.selectedLevel, 2) + " / UNLOCKED " +
                         number(g.unlockedLevel, 2));
        result.push_back("WHEEL SELECT / CLICK OR ENTER TO START");
        result.push_back(std::string("KEY ") + Game::keyNames[g.accessKey] + " / K CHANGE KEY");
        if (g.beastlyUnlocked)
            result.push_back(g.beastly ? "BEASTLY / DOUBLE SCORE / O NORMAL"
                                       : "O ENABLE BEASTLY MODE");
    } else if (g.mode == Mode::Victory) {
        result.push_back("SUPERSTAR / CAMPAIGN COMPLETE");
        result.push_back(g.beastly ? "TOTALLY AWESOME / BEASTLY COMPLETE"
                                   : "BEASTLY UNLOCKED / O AT LEVEL SELECT");
        result.push_back("ESC LEVEL SELECT / ENTER PLAY AGAIN");
    } else if (g.mode == Mode::GameOver) {
        result.push_back("GAME OVER / CLICK OR ENTER TO RETRY");
    } else {
        if (g.isBonus()) {
            result.push_back(std::string(g.bonusType == 1   ? "TUNNEL"
                                         : g.bonusType == 2 ? "PSYCHEDELIC"
                                                            : "RING") +
                             " BONUS / " + (g.bonusType == 1 ? "TURNS " : "GATES ") +
                             number(g.bonusType == 1 ? g.bonusGate : g.bonusHits) + "/" +
                             number(g.bonusGateCount()));
            char speed[80];
            std::snprintf(speed, sizeof(speed), "SPEED %.2fX / BOOSTS %d / LAST +%d",
                          g.bonusSpeed / (g.bonusType == 2 ? 37.5f : 30.f), g.bonusBoosts,
                          g.bonusLastScore);
            if (g.bonusType == 1)
                std::snprintf(speed, sizeof(speed), "TRAIL %.1f/31.5 / SCORE +%d / SPEED 60 SEG/S",
                              g.tunnelTrail, g.bonusLastScore);
            result.push_back(speed);
        }
        if (g.mode == Mode::Paused)
            result.push_back("PAUSED / CLICK OR ENTER TO RESUME");
        else if (g.isBonusReturn())
            result.push_back("RETURNING TO WEB");
        else if (g.isBonus())
            result.push_back(g.bonusType == 1 ? "WHEEL ADJUSTS SPEED / REVERSE TO BRAKE"
                                              : "WHEEL ADJUSTS DRIFT / REVERSE TO BRAKE");
        else if (g.deathTime > 0)
            result.push_back(g.snatched ? "SHIP CAPTURED / RETURNING TO WEB"
                                        : "SHIP DESTROYED / RETURNING TO WEB");
        else if (g.mode == Mode::Transition)
            result.push_back("WEB CLEAR");
        else
            result.push_back("WHEEL MOVE / RMB ZAP / MMB JUMP");
    }
    if (g.enteringScore) {
        result.push_back("HIGH SCORE / " + g.scoreInitials + " / LETTER " +
                         number(g.scoreCursor + 1));
        result.push_back("WHEEL LETTER / LMB NEXT / ENTER SAVE");
    }
    if (g.isBonus()) {
        auto instructions = bonusInstructions(g);
        result.insert(result.end(), instructions.begin(), instructions.end());
    }
    return result;
}
std::vector<std::string> Renderer::bonusInstructions(const Game &g) {
    if (!g.isBonus() || g.bonusFinished)
        return {};
    if (g.bonusType == 1 && g.bonusTime > 4 && g.mode != Mode::Paused)
        return {"STAY ON GREEN / REVERSE WHEEL TO BRAKE"};
    if (g.bonusType == 1)
        return {"STAY ON THE GREEN TRACK", "WHEEL / LEFT / RIGHT / A / D: ADJUST ROTATION SPEED",
                "REVERSE TO BRAKE / MOTION HAS INERTIA",
                "ON TRACK GROWS TRAIL / OFF TRACK SHRINKS IT",
                "RECOVER BEFORE TRAIL RUNS OUT / COMPLETE TO WARP"};
    if (g.bonusGate > 0 && g.mode != Mode::Paused)
        return {"LMB (A): UP / RMB (C): DOWN / REVERSE WHEEL TO BRAKE"};
    return {"AIM THROUGH RINGS / MISSING ENDS BONUS",
            "WHEEL: HORIZONTAL DRIFT / REVERSE WHEEL TO BRAKE", "HOLD LMB (A): UP / RMB (C): DOWN",
            "RELEASE TO COAST / UP-W AND DOWN-S ALSO WORK",
            "ARROWS PUSH / BOLTS BOOST / +20000 TO FINISH"};
}
void Renderer::headsetHud(const Game &g) {
    glDisable(GL_DEPTH_TEST);
    glPushMatrix();
    glTranslatef(-1.6f, 1.9f, -3.8f);
    glScalef(.0055f, -.0055f, .0055f);
    auto lines = xrHudLines(g);
    auto instructions = bonusInstructions(g);
    size_t instructionStart = lines.size() - instructions.size();
    for (size_t i = 0; i < lines.size(); ++i) {
        // Inventory stays above the rim; mode/bonus prompts sit below it.
        float y = i < 5                   ? float(i) * 22
                  : i >= instructionStart ? 120 + float(i - instructionStart) * 24
                                          : 630 + float(i - 5) * 24;
        text(0, y, lines[i], 1.4f, i < 2 ? white : i < 5 ? cyan : lime);
    }
    glPopMatrix();
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
    rect(48, 105, 1344, 1, {.2f, .5f, .65f, .3f});
    text(48, 26, "T / 2000", 3, cyan);
    text(48, 64, "VECTOR COMBAT SYSTEM", 1.4f, {.38f, .49f, .65f});
    text(460, 24, "SCORE", 1.4f, cyan);
    text(460, 49, number(g.score, 7), 3.5f, white);
    text(750, 24, "BEST", 1.4f, {.45f, .53f, .68f});
    text(750, 49, number(g.best, 7), 2.5f, {.6f, .68f, .8f});
    text(1040, 24, "WEB " + number(g.level, 2) + " / " + g.web.name, 1.5f, cyan);
    text(1040, 55, "LIVES " + number(std::max(0, g.lives)) + "  ZAP " + number(g.zaps), 2, lime);
    rect(48, 802, 1344, 1, {.2f, .5f, .65f, .3f});
    if (g.isBonus()) {
        text(48, 829,
             g.bonusType == 1 ? "WHEEL / LEFT / RIGHT: ROTATION SPEED / REVERSE TO BRAKE"
                              : "WHEEL: DRIFT / REVERSE TO BRAKE   LMB (A): UP   RMB (C): DOWN",
             1.6f, cyan);
    } else {
        text(48, 829, "WHEEL", 1.6f, white);
        text(116, 829, "MOVE", 1.6f, {.4f, .54f, .68f});
        text(242, 829, "LMB", 1.6f, white);
        text(291, 829, "HOLD TO FIRE", 1.6f, {.4f, .54f, .68f});
        text(511, 829, "RMB", 1.6f, pink);
        text(559, 829, "SUPERZAPPER", 1.6f, {.4f, .54f, .68f});
        text(771, 829, "MMB / SPACE", 1.6f, white);
        text(957, 829, "JUMP", 1.6f, {.4f, .54f, .68f});
    }
    text(48, 869, "P PAUSE   V XR   B STEREO   F11 FULLSCREEN", 1.25f, {.35f, .46f, .61f});
    char settings[160];
    std::snprintf(settings, sizeof(settings), "[ / ] WHEEL %.2f  I %s  M %s", sensitivity,
                  inverted ? "REVERSED" : "NORMAL", muted ? "MUTED" : "AUDIO");
    text(823, 869, settings, 1.1f, {.35f, .46f, .61f});
    text(48, 123, status, 1.25f, {.35f, .56f, .65f});
    if (g.isBonus()) {
        auto instructions = bonusInstructions(g);
        for (size_t i = 0; i < instructions.size(); ++i)
            text(48, 153 + float(i) * 23, instructions[i], 1.5f, amber);
    }
    if (g.mode == Mode::Menu) {
        rect(48, 191, 4, 31, cyan);
        text(71, 198, "RECONSTRUCTION / 01", 1.8f, cyan);
        if (!drawRaster("logo", 70, 255, 495, 186))
            text(67, 263, "TEMPEST", 11, white);
        sourceText(1, 70, 445, "2000", 11, white);
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
        text(720, 195, std::string("PLAYER ACCESS KEY ") + Game::keyNames[g.accessKey], 2, cyan);
        text(720, 228, "K CHANGE KEY / PROGRESS SAVED PER KEY", 1.4f, white);
        if (g.beastlyUnlocked)
            text(720, 266, g.beastly ? "O BEASTLY ON / DOUBLE SCORE" : "O ENABLE BEASTLY MODE",
                 1.6f, pink);
        int page = int(g.time / 12) % 3;
        if (page == 0) {
            text(720, 316, "TEMPEST DUDES / HIGH SCORES", 2, lime);
            for (size_t i = 0; i < g.highScores.size(); ++i) {
                const auto &entry = g.highScores[i];
                text(720, 362 + float(i) * 31,
                     number(int(i) + 1, 2) + "  " + entry.initials + "  " + number(entry.score, 8),
                     1.9f, i ? white : pink);
            }
        } else if (page == 1) {
            const char *tutorial[] = {"HOW TO PLAY",
                                      "WHEEL ROTATES AROUND THE RIM",
                                      "HOLD LMB TO FIRE",
                                      "RMB SUPERZAPPER / TWO CHARGES",
                                      "COLLECT LASER THEN JUMP THEN DROID",
                                      "MMB / SPACE JUMPS AFTER PICKUP",
                                      "THREE TOKENS OPEN A BONUS",
                                      "BONUS WHEEL ADDS DRIFT",
                                      "REVERSE WHEEL TO BRAKE",
                                      "RINGS: LMB UP / RMB DOWN"};
            for (int i = 0; i < 10; ++i)
                text(720, 316 + float(i) * 34, tutorial[i], i ? 1.5f : 2, i ? white : lime);
        } else {
            const char *credits[] = {"ORIGINAL TEMPEST 2000 / 1994",
                                     "GAME DESIGN / JEFF MINTER",
                                     "ORIGINAL TEMPEST / DAVE THEURER",
                                     "MUSIC / IMAGITEC DESIGN",
                                     "VOICES / TED AND CARRIE TAHQUECHI",
                                     "PRODUCER / JOHN SKRUCH",
                                     "ATARI CORPORATION",
                                     "TEMPEST XR / SOURCE RECREATION",
                                     "DESKTOP / OPENXR / ROTARY INPUT"};
            for (int i = 0; i < 9; ++i)
                text(720, 316 + float(i) * 36, credits[i], i ? 1.5f : 2, i ? white : lime);
        }
    } else {
        if (g.enteringScore) {
            text(575, 570, "HIGH SCORE " + g.scoreInitials, 3, lime);
            text(575 + 11 * 18 + float(g.scoreCursor) * 18, 599, "^", 3, pink);
            text(350, 643, "WHEEL LETTER / LMB NEXT / RMB BACK / ENTER SAVE", 1.8f, white);
        }
        rect(48, 775, 1344, 3, {.08f, .16f, .23f, 1});
        rect(48, 775, 1344 * g.progress(), 3, cyan);
        if (g.mode == Mode::Paused || g.mode == Mode::GameOver || g.mode == Mode::Transition ||
            g.mode == Mode::Victory) {
            std::string label = g.mode == Mode::Victory    ? "SUPERSTAR"
                                : g.mode == Mode::Paused   ? "PAUSED"
                                : g.mode == Mode::GameOver ? "GAME OVER"
                                                           : "WEB CLEAR";
            float x = 720 - float(label.size()) * 6 * 6 / 2;
            bool rasterTitle =
                g.mode == Mode::GameOver && drawRaster("gameover", 580, 348, 280, 131);
            if (!rasterTitle)
                text(x, 375, label, 6, g.mode == Mode::GameOver ? pink : cyan);
            std::string sub =
                g.mode == Mode::Victory    ? (g.beastly ? "BEASTLY COMPLETE / SIX CUPS OF TEA!"
                                                        : "BEASTLY UNLOCKED / ESC LEVEL SELECT")
                : g.mode == Mode::Paused   ? "P / ESC TO RESUME"
                : g.mode == Mode::GameOver ? (g.enteringScore ? "HIGH SCORE / ENTER INITIALS BELOW"
                                                              : "ENTER TO PLAY AGAIN")
                                           : "";
            text(720 - float(sub.size()) * 6 * 1.8f / 2, rasterTitle ? 510 : 462, sub, 1.8f, white);
        }
        if (g.isBonus()) {
            text(48, 751,
                 (g.bonusType == 1 ? "TURNS " : "GATES ") +
                     number(g.bonusType == 1 ? g.bonusGate : g.bonusHits) + "/" +
                     number(g.bonusGateCount()),
                 1.5f, cyan);
            char bonusStatus[96];
            std::snprintf(bonusStatus, sizeof(bonusStatus), "SPEED %.2fX / BOOSTS %d / LAST +%d",
                          g.bonusSpeed / (g.bonusType == 2 ? 37.5f : 30.f), g.bonusBoosts,
                          g.bonusLastScore);
            if (g.bonusType == 1)
                std::snprintf(bonusStatus, sizeof(bonusStatus),
                              "TRAIL %.1f/31.5 / LAST +%d / REVERSE WHEEL TO BRAKE", g.tunnelTrail,
                              g.bonusLastScore);
            text(500, 751, bonusStatus, 1.4f, lime);
        } else {
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
}
} // namespace tempest
