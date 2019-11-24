#pragma once

// satellite
#include <UI.hpp>
#include <Graphics.hpp>

class TextureVisualizer : public Panel {
public:
    TextureVisualizer(const PanelSharedPtr& panel);

    void init() override;
    void draw() override;

private:
    Pipeline    _pipeline;
    ImageHandle _texture;
};