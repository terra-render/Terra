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
    float _color_face[4] = { 1., 1., 1., 1. };
    float _color_edges[4] = { 1., 1., 1., 1. };
    float _color_face_selected[4] = { 1., 1., 1., 1. };
    float _color_edges_selected[4] = { 1., 1., 1., 1. };

    Pipeline _pipeline;
};