#pragma once

// libc++
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declarations
class GFXLayer;

enum PanelFlags {
    PANEL_FLAG_NONE = 0x0,
    PANEL_FLAG_HIDE = 1 << 1
};
using PanelFlagBits = int;

struct Panel {
    Panel(
        const std::shared_ptr<Panel>& parent = nullptr,
        const std::string& name = ""
    ) : parent(parent), name(name) { };

    std::shared_ptr<Panel> parent;
    const std::string   name;
    PanelFlagBits flags = PANEL_FLAG_NONE;

    virtual void init() = 0;
    virtual void draw() = 0;
};
using PanelSharedPtr = std::shared_ptr<Panel>;

/*
    A collection of panels
*/
class UI : public Panel {
public:
    UI(GFXLayer* gfx);

    void init() override;
    void add_panel (const PanelSharedPtr& panel);
    void draw() override;
    const PanelSharedPtr& find(const char* name);

private:
    GFXLayer* _gfx;
    std::unordered_map<std::string, PanelSharedPtr> _panels_map;
    std::vector<PanelSharedPtr> _panels;
};