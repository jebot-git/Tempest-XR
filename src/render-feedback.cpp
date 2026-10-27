#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include "cry.hpp"
#include "render.hpp"
#include <SDL_opengl.h>
#include <stdexcept>
namespace tempest {
Color Renderer::originalColor(int hue, int intensity, float alpha) {
    const auto &base = cryBases[size_t(hue & 255)];
    intensity = std::clamp(intensity, 0, 255);
    return {float(base[0] * intensity >> 8) / 255, float(base[1] * intensity >> 8) / 255,
            float(base[2] * intensity >> 8) / 255, alpha};
}
void Renderer::initFeedback() {
    if (feedbackProgram)
        return;
    // Quantize native RGB inputs once into a CRY working space. Feedback then
    // uses Jaguar hue-preserving, saturating intensity arithmetic.
    std::vector<uint8_t> encode(32 * 32 * 32 * 3);
    for (int b = 0; b < 32; ++b)
        for (int g = 0; g < 32; ++g)
            for (int r = 0; r < 32; ++r) {
                float rgb[3] = {r * 255.f / 31, g * 255.f / 31, b * 255.f / 31};
                float best = 1e20f;
                int hue = 0, intensity = 0;
                for (int h = 0; h < 256; ++h) {
                    const auto &base = cryBases[size_t(h)];
                    float dot = 0, norm = 0;
                    for (int c = 0; c < 3; ++c) {
                        dot += rgb[c] * base[c];
                        norm += base[c] * base[c];
                    }
                    int i = std::clamp(int(std::round(dot * 256 / std::max(1.f, norm))), 0, 255);
                    float error = 0;
                    for (int c = 0; c < 3; ++c) {
                        float d = rgb[c] - (base[c] * i >> 8);
                        error += d * d;
                    }
                    if (error < best) {
                        best = error;
                        hue = h;
                        intensity = i;
                    }
                }
                size_t at = size_t((b * 32 + g) * 32 + r) * 3;
                encode[at] = uint8_t(hue);
                encode[at + 1] = uint8_t(intensity);
            }
    glGenTextures(1, &cryEncodeTexture);
    glBindTexture(GL_TEXTURE_3D, cryEncodeTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 32, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, encode.data());
    glGenTextures(1, &cryPaletteTexture);
    glBindTexture(GL_TEXTURE_1D, cryPaletteTexture);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, cryBases.data());
    const char *vs = R"(#version 120
        varying vec2 uv;
        void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;})";
    const char *fs = R"(#version 120
        uniform sampler2D history; uniform sampler3D encoder; uniform sampler1D palette;
        uniform vec2 rotation, center; uniform float scale, intensity, alpha;
        varying vec2 uv;
        void main(){
            vec2 p=floor(uv*vec2(384.,240.))-vec2(192.,120.);
            p=vec2(p.x*rotation.y-p.y*rotation.x,p.x*rotation.x+p.y*rotation.y)/scale+center;
            vec2 tc=(floor(p)+.5)/vec2(384.,240.);
            if(tc.x<0.||tc.x>1.||tc.y<0.||tc.y>1.){gl_FragColor=vec4(0.,0.,0.,alpha);return;}
            vec3 old=texture2D(history,tc).rgb;
            vec3 cry=texture3D(encoder,(floor(old*31.+.5)+.5)/32.).rgb;
            float hue=floor(cry.r*255.+.5), i=clamp(floor(cry.g*255.+.5)+intensity,0.,255.);
            vec3 base=floor(texture1D(palette,(hue+.5)/256.).rgb*255.+.5);
            gl_FragColor=vec4(floor(base*i/256.)/255.,alpha);
        })";
    auto compile = [](GLenum type, const char *text) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            glGetShaderInfoLog(shader, 1024, nullptr, log);
            glDeleteShader(shader);
            throw std::runtime_error(log);
        }
        return shader;
    };
    GLuint vertex = compile(GL_VERTEX_SHADER, vs), fragment = compile(GL_FRAGMENT_SHADER, fs);
    feedbackProgram = glCreateProgram();
    glAttachShader(feedbackProgram, vertex);
    glAttachShader(feedbackProgram, fragment);
    glLinkProgram(feedbackProgram);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = 0;
    glGetProgramiv(feedbackProgram, GL_LINK_STATUS, &ok);
    if (!ok)
        throw std::runtime_error("Cannot link CRY feedback shader");
}
void Renderer::finishFrame(const Game &g, int view, int x, int y, int width, int height) {
    if (g.mode == Mode::Paused)
        return;
    auto &s = meltStates.at(size_t(view));
    bool changed = s.seeded && s.previous != g.mode &&
                   ((s.previous == Mode::Menu && g.mode == Mode::Playing) ||
                    g.mode == Mode::GameOver || g.mode == Mode::Menu || g.mode == Mode::Bonus ||
                    g.mode == Mode::Victory || g.isBonusReturn());
    changed = changed || (s.seeded && g.mode == Mode::Menu && s.previousPage != g.frontPage);
    if (changed) {
        s.started = g.time;
        s.startFrame = g.frameNumber();
    }
    bool resized = s.width != width || s.height != height;
    if (!s.texture) {
        glGenTextures(1, &s.texture);
        resized = true;
    }
    int age = int(g.frameNumber() - s.startFrame);
    bool pyramid = g.isBonus() && (g.bonusType == 2 || (g.bonusWon && g.bonusFinished));
    bool melt = g.time - s.started >= 0 && age <= 150;
    if (s.seeded && !resized && (melt || pyramid)) {
        initFeedback();
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(feedbackProgram);
        auto uniform = [&](const char *name) {
            return glGetUniformLocation(feedbackProgram, name);
        };
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s.texture);
        glUniform1i(uniform("history"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, cryEncodeTexture);
        glUniform1i(uniform("encoder"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_1D, cryPaletteTexture);
        glUniform1i(uniform("palette"), 2);
        int frame = int(g.frameNumber()),
            angle = pyramid ? (std::abs(((frame / 4) & 127) - 63) - 31) & 255 : 255;
        if (!pyramid && age >= 8)
            angle = 0;
        float steps = g.frameNumber() >= s.lastFrame ? float(g.frameNumber() - s.lastFrame) : 1.f;
        if (!steps)
            angle = 0;
        glUniform2f(uniform("rotation"), float(sourceSines[size_t(angle)]) / 128,
                    steps ? float(sourceSines[size_t((angle + 64) & 255)]) / 128 : 1.f);
        glUniform2f(uniform("center"),
                    pyramid ? 193 + float(sourceSines[size_t(frame & 255)]) / 32 : 192,
                    pyramid ? 124 + float(sourceSines[size_t(frame & 255)]) / 16 : 120);
        glUniform1f(uniform("scale"), std::pow((pyramid ? 494.f : 500.f) / 512, steps));
        glUniform1f(uniform("intensity"), (pyramid ? -1 : -8) * steps);
        glUniform1f(uniform("alpha"), pyramid ? .72f : .85f * std::max(0.f, 1 - float(age) / 151));
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
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);
        glPopAttrib();
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (resized)
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, width, height, 0);
    else
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);
    s.width = width;
    s.height = height;
    s.previous = g.mode;
    s.previousPage = g.frontPage;
    s.seeded = true;
    s.lastFrame = g.frameNumber();
    s.lastTime = g.time;
}
} // namespace tempest
