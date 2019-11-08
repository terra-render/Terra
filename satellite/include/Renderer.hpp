#pragma once

// C++ STL
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

// Terra
#include <Terra.h>

// Cloto
#include <Cloto.h>

#include <Graphics.hpp>
#include <Config.hpp>

class Renderer {
public:
    using Event = std::function<void() >;
    using TileEvent = std::function<void(size_t x, size_t y, size_t w, size_t h) >;

public:
    Renderer() { };
    virtual ~Renderer() = 0;

    virtual void clear() = 0;
    virtual void update() = 0;
    virtual void pause() = 0;
    virtual bool step(
        const TerraCamera& camera, 
        HTerraScene scene, 
        const Event& on_step_end, 
        const TileEvent& on_tile_begin, 
        const TileEvent& on_tile_end
    ) = 0;

    virtual void on_config_updated() = 0;

    virtual const TextureData& framebuffer() = 0;
};