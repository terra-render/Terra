#include <Renderer.hpp>

class WireframeRenderer : Renderer {
public:
    WireframeRenderer();
    ~WireframeRenderer();

    void clear() override;
    void update() override;
    void pause() override;
    bool step(
        const TerraCamera& camera,
        HTerraScene scene,
        const Event& on_step_end,
        const TileEvent& on_tile_begin,
        const TileEvent& on_tile_end
    ) override;

    virtual void on_config_updated() override;
    virtual const TextureData& framebuffer() override;
};