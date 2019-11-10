// satellite
#include <Renderer.hpp>

// libc++
#include <cassert>

Renderer::Renderer() :
    _is_camera_locked(false),
    _is_paused(true),
    _selected(Object::ID_NULL) {

}

void Renderer::start() {
    assert(_is_paused);
    _is_paused = false;
}

void Renderer::pause() {
    assert(!_is_paused);
    _is_paused = true;
}

void Renderer::update_settings(
    const Settings& settings
) {
    _settings = settings;
}