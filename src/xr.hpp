#pragma once
#include <memory>
#include <string>
struct SDL_Window;
namespace tempest {
class Renderer;
class Game;
class XR {
    struct Impl;
    std::unique_ptr<Impl> p;

  public:
    XR();
    ~XR();
    XR(const XR &) = delete;
    XR &operator=(const XR &) = delete;
    void initialize(SDL_Window *window);
    bool frame(Renderer &renderer, const Game &game);
    void recenter();
    void setVisible(bool visible);
    bool focused() const;
    std::string description() const;
};
} // namespace tempest
