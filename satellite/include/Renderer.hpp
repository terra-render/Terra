#pragma once

// C++ STL
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

// Terra
#include <Terra.h>

// Cloto
#include <Cloto.h>
#include <Config.hpp>
#include <Graphics.hpp>
#include <Config.hpp>
#include <Object.hpp>

class Scene;
class Camera;

class Renderer {
public:
    using Event = std::function<void() >;
    using TileEvent = std::function<void(size_t x, size_t y, size_t w, size_t h) >;
    using Settings = std::unordered_map<std::string, Config::Opt>;

public:
    Renderer();
    virtual ~Renderer() = default;

    void resize(const unsigned int width, const unsigned int height);

    virtual void clear() = 0;
    
    virtual void update(
        const Scene& scene,
        const Camera& camera
    ) = 0;

    virtual void start();
    virtual void pause();
    virtual void update_settings(
        const Settings& settings
    );

    const ImageID& render_target() const { return _render_target; }
    bool is_camera_locked() const { return _is_camera_locked; }
    bool is_paused()const { return _is_paused; }
    void set_selected(const Object::ID& id) { _selected = id; }
    const Settings& settings() { return _settings; }

protected:
    Settings    _settings;
    ImageID     _render_target;
    bool        _is_camera_locked;
    bool        _is_paused;
    Object::ID  _selected;
};