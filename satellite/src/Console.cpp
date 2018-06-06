// Header
#include <Console.hpp>

// Satellite
#include <Graphics.hpp>

// stdlib
#include <cstdarg>
#include <string>

// imgui
#include <imgui.h>

using namespace ImGui;
using namespace std;

#ifdef __cplusplus // Standard conformant name for c++
    #define strdup _strdup
#endif

Console::Console ( ) :
    _hidden ( false ),
    _font ( nullptr ),
    _width ( 0.f ),
    _height ( 0.f ) {
    clear();
    memset ( _input_buf, 0, sizeof ( _input_buf ) );
    _history_pos = -1;
}

Console::~Console() {
    clear();
}

void Console::set_font ( ImFont* font ) {
    _font = font;
}

void Console::printf ( const char* fmt, ... ) {
    va_list args;
    va_start ( args, fmt );
    vprintf ( fmt, args );
    va_end ( args );
}

void Console::vprintf ( const char* fmt, va_list args ) {
    constexpr size_t buf_len = 4096;
    char buf[buf_len + 1];
    vsnprintf ( buf, buf_len, fmt, args );
    buf[buf_len] = '\0';
    _items.push_back ( strdup ( buf ) );
    _scroll_to_bottom = true;
}

void Console::clear() {
    for ( int i = 0; i < ( int ) _items.size(); i++ ) {
        free ( _items[i] );
    }

    _items.clear();
    _scroll_to_bottom = true;
}

int Console::_text_edit_callback_stub ( ImGuiTextEditCallbackData* data ) {
    Console* console = ( Console* ) data->UserData;
    return console->_text_edit_callback ( data );
}

namespace ImGui {
    extern bool InputTextEx ( const char* label, char* buf, int buf_size, const ImVec2& size_arg, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void* user_data );
}

void Console::draw ( GFXLayer gfx ) {
    if ( _hidden ) {
        return;
    }

    if ( _font ) {
        PushFont ( _font );
    }

    PushStyleVar ( ImGuiStyleVar_WindowRounding, 0.f );
    // Anchoring bottom right corner
    int wnd_width = gfx_width ( gfx );
    int wnd_height = gfx_height ( gfx );
    _width = _width == 0.f ? 0.5f * wnd_width : _width;
    _height = _height == 0.f ? 0.5f * wnd_height : _height;
    SetNextWindowSize ( ImVec2 ( _width, _height ), ImGuiCond_Once );
    SetNextWindowPos ( ImVec2 ( wnd_width - _width, wnd_height - _height ) );

    if ( !Begin ( "Command Console", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize ) ) {
        End();
        return;
    }

    const float footer_height_to_reserve = GetStyle().ItemSpacing.y + GetFrameHeightWithSpacing(); // 1 separator, 1 input text
    BeginChild ( "ScrollingRegion", ImVec2 ( 0, -footer_height_to_reserve ), false, 0x0 ); // Leave room for 1 separator + 1 InputText

    if ( BeginPopupContextWindow() ) {
        if ( Selectable ( "Clear" ) ) {
            clear();
        }

        EndPopup();
    }

    PushStyleVar ( ImGuiStyleVar_ItemSpacing, ImVec2 ( 4, 1 ) );
    ImVec4 col_default_text = GetStyleColorVec4 ( ImGuiCol_Text );

    for ( int i = 0; i < ( int ) _items.size(); i++ ) {
        const char* item = _items[i];

        if ( strncmp ( item, "err", 3 ) == 0 ) {
            TextColored ( ImColor ( 255, 0, 0 ), item );
        } else if ( strncmp ( item, "warn", 4 ) == 0 ) {
            TextColored ( ImColor ( 244, 188, 66 ), item );
        } else if ( strncmp ( item, "info", 4 ) == 0 ) {
            TextColored ( ImColor ( 65, 244, 104 ), item );
        } else if ( strncmp ( item, "run", 3 ) == 0 ) {
            TextColored ( ImColor ( 67, 154, 216 ), item );
        } else {
            TextUnformatted ( item );
        }
    }

    TextUnformatted ( "" );

    if ( _scroll_to_bottom ) {
        SetScrollHere();
    }

    _scroll_to_bottom = false;
    PopStyleVar();
    EndChild();
    Separator();
    // Command-line
    bool reclaim_focus = false;
    auto trim = [] ( string & s ) {
        if ( s.empty() ) {
            return;
        }

        while ( isspace ( s.front() ) ) {
            s.erase ( s.begin() );
        }

        while ( isspace ( s.back() ) ) {
            s.pop_back();
        }
    };

    if ( InputTextEx ( "", _input_buf, IM_ARRAYSIZE ( _input_buf ), ImVec2 ( GetContentRegionAvailWidth(), 0 ),
                       ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                       &_text_edit_callback_stub, ( void* ) this ) ) {
        char* input_end = _input_buf + strlen ( _input_buf );

        while ( input_end > _input_buf && input_end[-1] == ' ' ) {
            input_end--;
        }

        *input_end = 0;

        if ( _callback ) {
            // Extracting arguments delimited by spaces
            CommandArgs args;
            char* cur = _input_buf;
            char* last = cur;

            while ( true ) {
                if ( *cur == ' ' || cur == input_end ) {
                    string arg ( last, cur - last );
                    trim ( arg );

                    if ( !arg.empty() ) {
                        args.push_back ( arg );
                    }

                    last = cur;
                }

                if ( cur == input_end ) {
                    break;
                }

                ++cur;
            }

            printf ( "run > %s", _input_buf );
            _history.push_back ( _strdup ( _input_buf ) );
            _history_pos = _history.size();

            // Calling user-defined function
            if ( !args.empty() ) {
                int callback_ret = _callback ( args );

                if ( callback_ret != 0 ) {
                    printf ( "`%s` failed with exit_code %d", args[0].c_str(), callback_ret );
                }
            }
        }

        strcpy ( _input_buf, "" );
        reclaim_focus = true;
    }

    if ( ImGui::IsItemHovered() || ( ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked ( 0 ) ) ) {
        ImGui::SetKeyboardFocusHere ( -1 );    // Auto focus previous widget
    }

    End();
    PopStyleVar ( 1 );

    if ( _font ) {
        PopFont();
    }
}

void Console::toggle() {
    _hidden = !_hidden;
}

int Console::_text_edit_callback ( ImGuiTextEditCallbackData* data ) {
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch ( data->EventFlag ) {
        case ImGuiInputTextFlags_CallbackHistory: {
            // Example of HISTORY
            const int prev_history_pos = _history_pos;

            if ( data->EventKey == ImGuiKey_UpArrow ) {
                if ( _history_pos == -1 ) {
                    _history_pos = ( int ) ( _history.size() - 1 );
                } else if ( _history_pos > 0 ) {
                    _history_pos--;
                }
            } else if ( data->EventKey == ImGuiKey_DownArrow ) {
                if ( _history_pos != -1 )
                    if ( ++_history_pos >= ( int ) _history.size() ) {
                        _history_pos = -1;
                    }
            }

            if ( prev_history_pos == _history.size() ) {
                _cur_cmd = _input_buf;
            }
        }
    }

    return 0;
}

void Console::set_callback ( const CommandCallback& callback ) {
    _callback = callback;
}