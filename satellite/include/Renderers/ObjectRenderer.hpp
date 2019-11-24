#pragma once

// satellite
#include <Renderer.hpp>
#include <Graphics.hpp>

// opengl
#include <GL\gl3w.h>

class ObjectRenderer : public Renderer {
public:
    ObjectRenderer();
    ~ObjectRenderer();

    void clear() override;
    void update(
        const Scene& scene,
        const Camera& camera
    ) override;
    void start() override;
    void pause() override;
    void update_settings(
        const Settings& settings
    ) override;

private:
    ImageHandle _rt_depth;
    Pipeline    _pipeline;
};