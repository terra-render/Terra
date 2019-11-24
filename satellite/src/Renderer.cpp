// satellite
#include <Renderer.hpp>
#include <Messages.hpp>

// libc++
#include <cassert>

Renderer::Renderer() :
    _is_camera_locked(false),
    _is_paused(true),
    _selected(Object::ID_NULL) {

    resize(1280, 720);

    MessageSetRenderView set_render_view;
    set_render_view.image = _render_target;
    SEND_MESSAGE(MSG_SET_RENDER_VIEW, MessageSetRenderView, set_render_view);
}

void Renderer::resize (
    const unsigned int width,
    const unsigned int height
) {
    _render_target = Image::create((int)width, (int)height, GL_RGBA8);
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