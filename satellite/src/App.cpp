// Header
#include <App.hpp>

// C++ STL
#include <cctype>
#include <fstream>

// Satellite
#include <Config.hpp>
#include <Logging.hpp>

// GLFW
#include <glfw/glfw3.h>

// Imgui
#include <imgui.h>
#include <examples/opengl3_example/imgui_impl_glfw_gl3.h>

#include <Commdlg.h>

using namespace std;

namespace {
    // Humanely formatted multiline string literal
    // I have not included R("") in the macro because of the intellisense
#define MULTILINE(str) ::usable_multiline(str)

    // Idea from https://stackoverflow.com/questions/1135841/c-multiline-string-literal?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    // As long as you don't mix spaces and tabs it should work also for them heretics
#define NOT_NULL(p)   (*(p) != '\0')
#define IS_NEWLINE(p) (*(p) == '\n' || *(p) == '\r')
    string usable_multiline ( const char* str ) {
        string ret;
        int    indent = 0;

        // Skipping first newlines
        while ( NOT_NULL ( str ) && IS_NEWLINE ( str ) ) {
            ++str;
        }

        // Findinding indentation of first line
        while ( NOT_NULL ( str ) && isspace ( *str ) && ++indent && ++str );

        while ( *str ) {
            const char* line_end = str;

            // Going to end of line
            while ( NOT_NULL ( line_end ) && !IS_NEWLINE ( line_end ) ) {
                ++line_end;
            }

            // Appending line
            ret.insert ( ret.end(), str, line_end );
            ret.push_back ( '\n' );

            // Skipping newline
            while (  NOT_NULL ( line_end ) && IS_NEWLINE ( line_end ) ) {
                ++line_end;
            }

            // Skipping indentation on next line
            str = line_end;
            int i = 0;

            while ( i++ < indent && NOT_NULL ( str ) && isspace ( *str ) ) {
                ++str;
            }
        }

        return ret;
    }

    // Macro so that we can do a couple of path combinations without
    // having to manipulate strings
#define DEFAULT_UI_FONT "Inconsolata.ttf"

    bool file_exists ( const char* filename ) {
        return std::ifstream ( filename ).good();
    }

    string find_default_ui_font() {
        if ( file_exists ( DEFAULT_UI_FONT ) ) {
            return DEFAULT_UI_FONT;
        }

        if ( file_exists ( "../" DEFAULT_UI_FONT ) ) {
            return "../" DEFAULT_UI_FONT;
        }

        if ( file_exists ( "fonts/" DEFAULT_UI_FONT ) ) {
            return "fonts/" DEFAULT_UI_FONT;
        }

        if ( file_exists ( "../fonts/" DEFAULT_UI_FONT ) ) {
            return "../fonts/" DEFAULT_UI_FONT;
        }

        return "";
    }
}

App::App ( int argc, char** argv ) {
    Log::set_targets ( stdout, stderr, stderr, stdout );
    Config::init();
}

App::~App() {
}

int App::run() {
    _gfx = gfx_init ( 1600, 900, "Satellite",
    [ = ] ( int w, int h ) { // on resize
        _on_resize ( w, h );
    },
    [ = ] ( const ImGuiIO & io ) { // on key
        if ( io.KeysDown[GLFW_KEY_GRAVE_ACCENT] && io.KeysDownDuration[GLFW_KEY_GRAVE_ACCENT] == 0 ) {
            _console.toggle();
        }
    } );

    if ( !_gfx ) {
        return EXIT_FAILURE;
    }

    _init_ui();
    Log::flush();
    _renderer.init ( 1600, 900, Config::read_i ( Config::JOB_TILE_SIZE ), Config::read_i ( Config::JOB_N_WORKERS ) );
    _visualizer.init ( _gfx );
    _register_commands();
    _boot();

    while ( !gfx_should_quit ( _gfx ) ) {
        gfx_process_events ( _gfx );
        _update();
        _draw();
        gfx_swap_buffers ( _gfx );
    }

    return EXIT_SUCCESS;
}

void App::_update() {
    _renderer.refresh_jobs();
}

// Imgui being/end are here as both the Visualizer and UI use it.
void App::_draw() {
    ImGui_ImplGlfwGL3_NewFrame();
    glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glClearColor ( 0.15f, 0.15f, 0.15f, 1.0f );
    _visualizer.draw();
    _console.draw ( _gfx );
    ImGui::Render();
    ImGui_ImplGlfwGL3_RenderDrawData ( ImGui::GetDrawData() );
}

int App::_on_command ( const CommandArgs& args ) {
    if ( args.empty() ) {
        return 1;
    }

    if ( _c_map.find ( args[0] ) == _c_map.end() ) {
        Log::error ( FMT ( "No command registered for: %s", args[0].c_str() ) );
        return 1;
    }

    CommandArgs args_only ( args.begin() + 1, args.end() );
    return _c_map[args[0]] ( args_only );
}

void App::_on_resize ( int width, int height ) {
    _renderer.resize ( width, height );
}

void App::_init_ui() {
    // Initializing ImGui (TODO: move to app?)
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ( void ) io;
    ImGui_ImplGlfwGL3_Init ( ( GLFWwindow* ) gfx_get_window ( _gfx ), true );
    ImGui::StyleColorsDark();
    io.IniFilename = nullptr;
    string font_file = find_default_ui_font();

    if ( font_file.empty() ) {
        io.Fonts->AddFontDefault();
    } else {
        io.Fonts->AddFontFromFileTTF ( font_file.c_str(), 15.f );
    }

    Log::redirect_console ( &_console );
    // Registerig command callback
    _console.set_callback ( std::bind ( &App::_on_command, this, std::placeholders::_1 ) );
}

void App::_register_commands() {
    //
    // help
    //
    _c_help = [this] ( const CommandArgs & args ) -> int {
        Log::console ( "Available commands. Enter command name for more information." );

        for ( const auto& cmd : _c_map ) {
            Log::console ( ( string ( "\t - " ) +  cmd.first ).c_str() );
        }

        return 0;
    };
    //
    // load
    //
    _c_load = [ this ] ( const CommandArgs & args ) -> int {
        char name[256];

        if ( args.size() < 1 ) {
            OPENFILENAMEA ofn;
            ZeroMemory ( &ofn, sizeof ( ofn ) );
            name[0] = '\0';
            ofn.lStructSize = sizeof ( OPENFILENAME );
            ofn.hwndOwner = GetFocus();
            ofn.lpstrFilter = NULL;
            ofn.lpstrCustomFilter = NULL;
            ofn.nMaxCustFilter = 0;
            ofn.nFilterIndex = 0;
            ofn.lpstrFile = name;
            ofn.nMaxFile = sizeof ( name );
            ofn.lpstrInitialDir = ".";
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrTitle = "Open OBJ file";
            ofn.lpstrDefExt = NULL;
            ofn.Flags = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;
            bool result = GetOpenFileName ( &ofn );

            if ( result == 0 ) {
                Log::console ( "<obj [path]> " );
                return 0;
            }
        } else {
            strcpy ( name, args[0].c_str() );
        }

        if ( !_scene.load ( name ) ) {
            return 1;
        }

        _render_camera = _scene.default_camera();
        _visualizer.info().scene = _scene.name();

        return 0;
    };
    //
    // step
    //
    _c_step = [ this ] ( const CommandArgs & args ) -> int {
        bool ret = _renderer.step ( &_render_camera, _scene.construct_terra_scene(),
        [ = ]() {
            _visualizer.info().sampling = Scene::from_terra_sampling ( _renderer.options().sampling_method );
            _visualizer.info().accelerator = Scene::from_terra_accelerator ( _renderer.options().accelerator );
            _visualizer.info().spp = ( int ) _renderer.options().samples_per_pixel;

            if ( !_renderer.is_progressive() ) {
                _visualizer.set_texture_data ( _renderer.framebuffer() );
            }
        },
        nullptr,
        [ = ] ( size_t x, size_t y, size_t w, size_t h ) {
            if ( _renderer.is_progressive() ) {
                _visualizer.update_tile ( _renderer.framebuffer(), x, y, w, h );
            }
        } );

        if ( !ret ) {
            Log::console ( STR ( "Failed to start renderer" ) );
            return 1;
        }

        return 0;
    };
    //
    // loop
    //
    _c_loop = [ this ] ( const CommandArgs & args ) -> int {
        bool ret = _renderer.loop ( &_render_camera, _scene.construct_terra_scene(),
        [ = ]() {
            if ( !_renderer.is_progressive() ) {
                _visualizer.set_texture_data ( _renderer.framebuffer() );
            }
        },
        nullptr,
        [ = ] ( size_t x, size_t y, size_t w, size_t h ) {
            if ( _renderer.is_progressive() ) {
                _visualizer.update_tile ( _renderer.framebuffer(), x, y, w, h );
            }
        } );

        if ( !ret ) {
            Log::console ( "Failed to start renderer" );
            return 1;
        }

        return 0;
    };
    //
    // pause
    //
    _c_pause = [ this ] ( const CommandArgs & args ) -> int {
        _renderer.pause_at_next_step();
        return 0;
    };
    //
    // save
    // TODO: Eventually add support for multiple outputs
    //
    _c_save = [ this ] ( const CommandArgs & args ) -> int {
        if ( args.size() < 1 ) {
            Log::console ( "<path>" );
            return 0;
        }

        const char* path = args[0].c_str();
        _visualizer.save_to_file ( path );

        return 0;
    };
    //
    // toggle
    //
    _c_toggle = [this] ( const CommandArgs & args ) -> int {
        string usage = MULTILINE ( R"(
            [console|info|stats])" );

        if ( args.size() < 1 ) {
            Log::console ( usage.c_str() );
            return 0;
        }

        if ( args[0].compare ( "console" ) == 0 ) {
            _console.toggle();
        } else if ( args[0].compare ( "info" ) == 0 ) {
            _visualizer.toggle_info();
        } else {
            Log::console ( "Unrecognized toggle" );
            return 1;
        }

        return 0;
    };
    //
    // option
    //
    _c_option = [ this ] ( const CommandArgs & args ) -> int {
        string usage = MULTILINE ( R"(
            list               - List all available options
            reset              - Reset options to defaults
            set <value> <name> - Set option value ( see list for types) )" );

        if ( args.size() < 1 ) {
            Log::console ( usage.c_str() );
            return 0;
        }

        if ( args[0].compare ( "list" ) == 0 ) {
            _scene.dump_opts();
            Log::info ( FMT ( "workers           = %d", _renderer.concurrent_jobs() ) );
            Log::info ( FMT ( "tile_size         = %d", _renderer.tile_size() ) );
        } else if ( args[0].compare ( "reset" ) == 0 ) {
            _scene.reset_options();
            Log::info ( STR ( "Reset options to Config default" ) );
        } else if ( args[0].compare ( "set" ) == 0 ) {
            if ( args.size() < 3 ) {
                Log::error ( STR ( "Expected <name> <value> pair." ) );
                return 1;
            }

            int opt = Config::find ( args[1].c_str() );

            // We don't want to lookup the options here
            if ( !_scene.set_opt ( opt, args[2].c_str() ) ) {
                int v;

                if ( Config::parse_i ( args[2].c_str(), v ) ) {
                    if ( opt == Config::JOB_N_WORKERS ) {
                        _renderer.set_concurrent_jobs ( v );
                        goto success;
                    } else if ( opt == Config::JOB_TILE_SIZE ) {
                        _renderer.set_tile_size ( v );
                        goto success;
                    }
                }

                Log::error ( FMT ( "Failed to find any matching option to %s", args[1].c_str() ) );
                return 1;
            }
        } else {
            Log::error ( STR ( "Unrecognized command" ) );
            return 1;
        }

success:
        return 0;
    };
    //
    // resize
    //
    _c_resize = [ this ] ( const CommandArgs & args ) -> int {
        if ( args.size() < 2 ) {
            Log::console ( "<width> <height>" );
            return 1;
        }

        int width = strtol ( args[0].c_str(), nullptr, 10 );
        int height = strtol ( args[1].c_str(), nullptr, 10 );

        if ( width == 0 || height == 0 ) {
            Log::console ( "Unrecognized command" );
            return 1;
        }

        gfx_resize ( _gfx, width, height );
        _on_resize ( width, height );

        Log::console ( "New resolution %d %d", width, height );

        return 0;
    };
    _c_clear = [this] ( const CommandArgs & args ) -> int {
        _console.clear();
        return 0;
    };
    _c_map["clear"]  = _c_clear;
    _c_map["help"]   = _c_help;
    _c_map["load"]   = _c_load;
    _c_map["step"]   = _c_step;
    _c_map["loop"]   = _c_loop;
    _c_map["pause"]  = _c_pause;
    _c_map["save"]   = _c_save;
    _c_map["toggle"] = _c_toggle;
    _c_map["option"] = _c_option;
    _c_map["opt"]    = _c_option;
    _c_map["resize"] = _c_resize;
    _c_map["hide"] = [this] ( const CommandArgs & args ) -> int {
        _console.toggle();
        return 0;
    };
}


void App::_boot() {
    _c_load ( { "../scenes/debug/textures/floor.obj" } );
}