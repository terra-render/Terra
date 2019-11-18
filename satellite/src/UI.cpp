// Satellite
#include <UI.hpp>
#include <OS.hpp>
#include <Graphics.hpp>

// Imgui
#include <imgui.h>
#include <examples/opengl3_example/imgui_impl_glfw_gl3.h>

#define DEFAULT_UI_FONT "Inconsolata.ttf"

using namespace std;

namespace {
    string find_default_ui_font() {
        if (OS::file_exists(DEFAULT_UI_FONT)) {
            return DEFAULT_UI_FONT;
        }

        if (OS::file_exists("../" DEFAULT_UI_FONT)) {
            return "../" DEFAULT_UI_FONT;
        }

        if (OS::file_exists("fonts/" DEFAULT_UI_FONT)) {
            return "fonts/" DEFAULT_UI_FONT;
        }

        if (OS::file_exists("../fonts/" DEFAULT_UI_FONT)) {
            return "../fonts/" DEFAULT_UI_FONT;
        }

        return "";
    }
}

UI::UI(GFXLayer* gfx) : _gfx (gfx) {

}

void UI::init() {
    // Initializing ImGui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplGlfwGL3_Init((GLFWwindow*)_gfx->get_window(), true);
    ImGui::StyleColorsDark();
    io.IniFilename = nullptr;
    string font_file = find_default_ui_font();

    if (font_file.empty()) {
        io.Fonts->AddFontDefault();
    }
    else {
        io.Fonts->AddFontFromFileTTF(font_file.c_str(), 15.f);
    }

    for (auto& panel : _panels) {
        panel->init();
    }
}

void UI::add_panel(const PanelSharedPtr& panel) {
    _panels.emplace_back(panel);
    assert(_panels_map.find(panel->name) == _panels_map.end());
    _panels_map[panel->name] = panel;
}

void UI::draw() {
    ImGui_ImplGlfwGL3_NewFrame();

    for (auto& panel : _panels) {
        if (panel->flags & PANEL_FLAG_HIDE) {
            continue;
        }

        panel->draw();
    }

    ImGui::Render();
    ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
}

const PanelSharedPtr& UI::find(const char* name) {
    assert(_panels_map.find(name) != _panels_map.end());
    return _panels_map[name];
}