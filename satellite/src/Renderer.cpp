// satellite
#include <Renderer.hpp>

// libc++
#include <cassert>

Renderer::Renderer() :
    _is_camera_locked(false),
    _is_paused(true),
    _selected(Object::ID_NULL) {

    resize(500, 500);

}

void Renderer::resize (
    const unsigned int width,
    const unsigned int height
) {
    glCreateTextures(GL_TEXTURE_2D, 1, &_render_target); GL_NO_ERROR;
    glTextureParameteri(_render_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); GL_NO_ERROR;
    glTextureParameteri(_render_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER); GL_NO_ERROR;
    glTextureParameteri(_render_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST); GL_NO_ERROR;
    glTextureParameteri(_render_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); GL_NO_ERROR;
    glTextureStorage2D(_render_target, 1, GL_RGBA8, (GLsizei)width, (GLsizei)height); GL_NO_ERROR;
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