#pragma once

// stdlib
#include <vector>
#include <functional>

// Satellite
#include <Graphics.hpp>

class Console;

using CommandArgs = std::vector<std::string>;
using CommandCallback = std::function<int ( const CommandArgs& args ) >;

struct ImGuiTextEditCallbackData;
struct ImFont;

// Adapted from Imgui's example
class Console {
  public:
    Console ();
    ~Console();

    void set_font ( ImFont* font );

    void printf ( const char* fmt, ... );
    void vprintf ( const char* fmt, va_list args );
    void clear();
    void draw ( GFXLayer gfx );

    // show/hide
    void toggle();

    void set_callback ( const CommandCallback& callback );

  private:
    static int _text_edit_callback_stub ( ImGuiTextEditCallbackData* data );
    int        _text_edit_callback ( ImGuiTextEditCallbackData* data );

    // Display Info
    bool      _hidden;
    ImFont*   _font;
    float     _width, _height;

    // Internal state
    char                     _input_buf[256];
    std::vector<char*>       _items;
    bool                     _scroll_to_bottom;
    std::vector<std::string> _history;
    std::string              _cur_cmd;
    int                      _history_pos;    // -1: new line, 0..History.Size-1 browsing history.
    std::vector<const char*> _commands;
    CommandCallback          _callback;
};
