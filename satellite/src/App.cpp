// Header
#include <App.hpp>

// C++ STL
#include <cctype>
#include <fstream>

// Satellite
#include <Config.hpp>
#include <Logging.hpp>
#include <Renderers/TerraRenderer.hpp>
#include <Renderers/ObjectRenderer.hpp>
#include <Panels/Console.hpp>
#include <Panels/Visualizer.hpp>
#include <Panels/RendererControls.hpp>
#include <Panels/TextureVisualizer.hpp>
#include <Camera.hpp>
#include <Messenger.hpp>
#include <Messages.hpp>
#include <CameraControls.hpp>
#include <UI.hpp>

// Terra
#include <TerraPresets.h>
#include <TerraProfile.h>

// GLFW
#include <glfw/glfw3.h>

#include <Commdlg.h>

using namespace std;

/*
    List of command names (`option list` from the terminal)
*/
#define CMD_CLEAR_NAME "clear"
#define CMD_HELP_NAME "help"
#define CMD_LOAD_NAME "load"
#define CMD_STEP_NAME "step"
#define CMD_LOOP_NAME "loop"
#define CMD_PAUSE_NAME "pause"
#define CMD_SAVE_NAME "save"
#define CMD_OPTION_NAME "opt"
#define CMD_OPTION_LIST_NAME "list"
#define CMD_OPTION_RESET_NAME "reset"
#define CMD_OPTION_SET_NAME "set"
#define CMD_OPTION_LOAD_NAME "load"
#define CMD_OPTION_SAVE_NAME "save"
#define CMD_RESIZE_NAME "resize"
#define CMD_HIDE_NAME "hide"
#define CMD_STATS_NAME "stats"
#define CMD_MESH_NAME "mesh"
#define CMD_MESH_LIST_NAME "list"
#define CMD_MESH_MOVE_NAME "move"


namespace {
    // Human friendly formatted multiline string literal
    // I have not included R("") in the macro because of the intellisense
#define MULTILINE(str) ::usable_multiline(str)

    // Idea from https://stackoverflow.com/questions/1135841/c-multiline-string-literal?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    // As long as you don't mix spaces and tabs it should work for both
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
}

App* key_receiver = nullptr;
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    assert(key_receiver);
}

App::App ( int argc, char** argv ) {
    Log::set_targets ( stdout, stderr, stderr, stdout );
    Config::init();
}

App::~App() { }

void App::_set_ui() {
    _ui = shared_ptr<Panel>(new UI(&_gfx));
    ((UI*)_ui.get())->add_panel(shared_ptr<Panel>((Panel*)(new TextureVisualizer(_ui))));
    ((UI*)_ui.get())->add_panel(shared_ptr<Panel>((Panel*)(new Console(_ui))));
    ((UI*)_ui.get())->add_panel(shared_ptr<Panel>((Panel*)(new RendererControls(_ui))));
    _ui->init();
}
   
void App::_set_renderer(const string& type) {
    SEND_MESSAGE(MSG_DETACH_RENDER_VIEW, MessageDetachRenderView, MessageDetachRenderView());

    if (type.compare(RENDER_OPT_RENDERER_OBJECT) == 0) {
        _renderer.reset(new ObjectRenderer);
    }
    else if (type.compare(RENDER_OPT_RENDERER_TERRA) == 0) {
        _renderer.reset(new TerraRenderer);
    }
    else {
        assert(false);
    }

    if (_renderer) {
        _renderer->start();
    }
}

void App::_set_camera(const string& type) {
    OrthographicCamera* camera = new OrthographicCamera;
    
    vec3 position, direction;
    vec3_set(position, 5.f, 5.f, 5.f);
    vec3_set(direction, -1.f, -1.f, -1.f);
    camera->set_lookat(position, direction);
    camera->resize(1280, 720);

    _camera.reset(camera);
}

void App::_set_camera_controls(const std::string& type) {
    _camera_controls.reset(new FirstPersonControls);
}

void App::_clear() {
    if (_renderer) {
        _renderer->clear();
    }
}

int App::run () {
    int boot_result = _boot();

    if ( boot_result != EXIT_SUCCESS ) {
        _shutdown();
        return boot_result;
    }

    _register_message_listener();
    _time_prev = glfwGetTime();

    while ( !_gfx.should_quit () ) {
        Messenger::dispatch();

        const double _time_now = glfwGetTime();
        _dt = _time_now - _time_prev;
        _time_prev = _time_now;

        // i/o
        _gfx.process_events ();

        // update camera
        if (_renderer && !_renderer->is_camera_locked()) {
            assert(_camera.get());
            assert(_camera_controls.get());
            _camera_controls->update(_gfx.window_handle(), *_camera.get(), _dt);
        }

        // Use active renderer to a offscreen target
        if (_renderer && !_renderer->is_paused()) {
            assert(_camera.get());
            _renderer->update(_scene, *_camera);
        }

        // Draw interface
        assert(_ui);
        _ui->draw();

        // present
        _gfx.swap_buffers ();
    }

    _shutdown();
    return EXIT_SUCCESS;
}

#if 0
void App::_init_ui() {
    Console* console = (Console*)(((UI*)_ui.get())->find("console").get());

    Log::redirect_console ( console );
    console->set_callback ( [this] ( const CommandArgs & args ) -> int {
        if ( args.empty() ) {
            return 1;
        }

        if ( _c_map.find ( args[0] ) == _c_map.end() ) {
            Log::error ( FMT ( "No command registered for: %s", args[0].c_str() ) );
            return 1;
        }

        CommandArgs args_only ( args.begin() + 1, args.end() );
        return _c_map[args[0]] ( args_only );
    } );
}
#endif

#if 0

void App::_init_cmd_map() {
    // help
    auto cmd_help = [this] ( const CommandArgs & args ) -> int {
        Log::console ( "Enter command name for more information." );

        for ( const auto& cmd : _c_map ) {
            Log::console ( ( string ( "\t - " ) +  cmd.first ).c_str() );
        }

        return 0;
    };

    // load
    auto cmd_load = [ this ] ( const CommandArgs & args ) -> int {
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
                return 0;
            }
        } else {
            strcpy ( name, args[0].c_str() );
        }

        _opt_set ( true, [this, name]() {
            if ( !_scene.load ( name ) ) {
                return 1;
            }

            _visualizer.info().scene = _scene.name();
            Config::write_s ( Config::RENDER_SCENE_PATH, name );
            return 0;
        } );

        return 0;
    };

    // step
    auto cmd_step = [ this ] ( const CommandArgs & args ) -> int {
        //bool ret = _renderer->ste( _scene.get_camera(), _scene.construct_terra_scene(),
        //[ = ]() {
        //    _visualizer.update_stats();
        //
        //    if ( !Config::read_i ( Config::VISUALIZER_PROGRESSIVE ) ) {
        //        _visualizer.set_texture_data ( _renderer->render_target() );
        //    }
        //},
        //nullptr,
        //[ = ] ( size_t x, size_t y, size_t w, size_t h ) {
        //    if ( Config::read_i ( Config::VISUALIZER_PROGRESSIVE ) ) {
        //        _visualizer.update_tile ( _renderer->render_target(), x, y, w, h );
        //    }
        //} );

        //if ( !ret ) {
        //    Log::error ( STR ( "Failed to start renderer" ) );
        //    return 1;
        //}

        return 0;
    };

    // loop
    auto cmd_loop = [ this ] ( const CommandArgs & args ) -> int {
        //bool ret = _renderer.loop ( _scene.get_camera(), _scene.construct_terra_scene(),
        //[ = ]() {
        //    _visualizer.update_stats();
        //
        //    if ( !Config::read_i ( Config::VISUALIZER_PROGRESSIVE ) ) {
        //        _visualizer.set_texture_data ( _renderer->render_target() );
        //    }
        //},
        //nullptr,
        //[ = ] ( size_t x, size_t y, size_t w, size_t h ) {
        //    if ( Config::read_i ( Config::VISUALIZER_PROGRESSIVE ) ) {
        //        _visualizer.update_tile ( _renderer->render_target(), x, y, w, h );
        //    }
        //} );
        //
        //if ( !ret ) {
        //    Log::error ( STR ( "Failed to start renderer" ) );
        //    return 1;
        //}

        return 0;
    };
    // pause
    auto cmd_pause = [ this ] ( const CommandArgs & args ) -> int {
        if (_renderer) {
            _renderer->pause();
        }
        return 0;
    };
    // save
    // TODO: support multiple outputs
    auto cmd_save = [ this ] ( const CommandArgs & args ) -> int {
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
            ofn.lpstrTitle = "Save image";
            ofn.lpstrDefExt = NULL;
            ofn.Flags = OFN_NOCHANGEDIR;
            bool result = GetSaveFileName ( &ofn );

            if ( result == 0 ) {
                return 0;
            }
        } else {
            strcpy ( name, args[0].c_str() );
        }

        _visualizer.save_to_file ( name );

        return 0;
    };
    // toggle
    /*auto cmd_toggle = [this] ( const CommandArgs & args ) -> int {
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
            Log::console ( usage.c_str() );
            return 1;
        }

        return 0;
    };*/
    // option
    auto cmd_option = [ this ] ( const CommandArgs & args ) -> int {
        string usage = MULTILINE ( R"(
            list               - List all available options
            reset              - Reset options to defaults
            set <value> <name> - Set option value ( see list for types) )" );

        if ( args.size() < 1 ) {
            Log::console ( usage.c_str() );
            return 0;
        }

        // Running option subcommand
        if ( args[0].compare ( CMD_OPTION_LIST_NAME ) == 0 ) {
            Config::dump_desc();
        } else if ( args[0].compare ( CMD_OPTION_RESET_NAME ) == 0 ) {
            _opt_set ( true, [this]() {
                Config::reset_to_default();
                return 0;
            } );
            Log::info ( STR ( "Reset options to Config default" ) );
        } else if ( args[0].compare ( CMD_OPTION_SET_NAME ) == 0 ) {
            if ( args.size() < 3 ) {
                Log::error ( STR ( "Expected <name> <value> pair." ) );
                return 1;
            }

            // Validate opt
            int opt = Config::find ( args[1].c_str() );

            if ( opt == -1 ) {
                Log::error ( FMT ( "Failed to find any matching option for %s", args[1].c_str() ) );
                return 1;
            }

            if ( opt == Config::RENDER_SCENE_PATH ) {
                Log::error ( FMT ( "load <path> to load a new scene." ) );
                return 1;
            }

            if ( args.size() == 5 ) {
                // value is float3
                char* arg = ( char* ) malloc ( args[2].length() + args[3].length() + args[4].length() + 2 );
                char* p = arg;
                strcpy ( p, args[2].c_str() );
                p += args[2].length();
                *p++ = ' ';
                strcpy ( p, args[3].c_str() );
                p += args[3].length();
                *p++ = ' ';
                strcpy ( p, args[4].c_str() );
                _opt_set ( opt, arg );
                free ( arg );
            } else {
                _opt_set ( opt, args[2] );
            }
        } else if ( args[0].compare ( CMD_OPTION_LOAD_NAME ) == 0 ) {
            if ( args.size() == 1 ) {
                if ( !Config::load() ) {
                    Log::error ( STR ( "No satellite.config file found while looking at default dirs." ) );
                    return 1;
                }
            } else {
                if ( Config::load ( args[1].c_str() ) ) {
                    Log::error ( STR ( "File not found." ) );
                    return 1;
                }
            }

            goto success;
        } else if ( args[0].compare ( CMD_OPTION_SAVE_NAME ) == 0 ) {
            if ( args.size() == 1 ) {
                if ( !Config::save() ) {
                    return 1;
                }
            } else {
                if ( !Config::save ( args[1].c_str() ) ) {
                    return 1;
                }
            }

            goto success;
        } else {
            Log::error ( STR ( "Unrecognized command" ) );
            return 1;
        }

success:
        return 0;
    };
    // resize
    auto cmd_resize = [ this ] ( const CommandArgs & args ) -> int {
        if ( args.size() < 2 ) {
            Log::console ( "<width> <height>" );
            return 1;
        }

        int width = strtol ( args[0].c_str(), nullptr, 10 );
        int height = strtol ( args[1].c_str(), nullptr, 10 );

        if ( width == 0 || height == 0 ) {
            Log::error ( STR ( "Can't set width or height to zero" ) );
            return 1;
        }

        _opt_set ( true, [this, width, height]() {
            Config::write_i ( Config::RENDER_WIDTH, width );
            Config::write_i ( Config::RENDER_HEIGHT, height );
            Log::console ( "New resolution %d %d", width, height );
            return 0;
        } );

        return 0;
    };
    // clear
    auto cmd_clear = [this] ( const CommandArgs & args ) -> int {
        _clear();
        return 0;
    };
    // hide
    auto cmd_hide = [this] ( const CommandArgs & args ) {
        _console.toggle();
        return 0;
    };
    // stats
    auto cmd_stats = [this] ( const CommandArgs & args ) {
#ifdef TERRA_PROFILE

        if ( _visualizer.stats().size() == 0 ) {
            _visualizer.add_stats_tracker ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RENDER, "render" );
            _visualizer.add_stats_tracker ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_TRACE, "trace" );
            _visualizer.add_stats_tracker ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY, "ray" );
            _visualizer.add_stats_tracker ( TERRA_PROFILE_SESSION_DEFAULT, TERRA_PROFILE_TARGET_RAY_TRIANGLE_INTERSECTION, "ray-triangle isect" );
        } else {
            _visualizer.remove_all_stats_trackers();
        }

#else
        Log::warning ( STR ( "Profiling disabled. Define TERRA_PROFILE and recompile to enable." ) );
#endif
        return 0;
    };
    auto cmd_mesh = [this] ( const CommandArgs & args ) {
        string usage = MULTILINE ( R"(
            list                    - List all meshes
            move <name> <x> <y> <z> - Move mesh (position refers to mesh center)" );

        if ( args.size() < 1 ) {
            Log::console ( usage.c_str() );
            return 1;
        }

        if ( args[0].compare ( CMD_MESH_LIST_NAME ) == 0 ) {
            std::vector<Scene::ObjectState> meshes;
            _scene.get_mesh_states ( meshes );

            for ( size_t i = 0; i < meshes.size(); ++i ) {
                Log::console ( "%s %f %f %f", meshes[i].name, meshes[i].x, meshes[i].y, meshes[i].z );
            }
        } else if ( args[0].compare ( CMD_MESH_MOVE_NAME ) == 0 ) {
            if ( args.size() < 5 ) {
                Log::error ( STR ( "mesh move <name> <x> <y> <z>" ) );
                return 1;
            }

            if ( !_scene.mesh_exists ( args[1].c_str() ) ) {
                Log::error ( STR ( "Invalid mesh name." ) );
                return 1;
            }

            // Moving is not an anctual option but the code works out anyway.
            _opt_set ( true, [this, args]() {
                float x = strtof ( args[2].c_str(), nullptr );
                float y = strtof ( args[3].c_str(), nullptr );
                float z = strtof ( args[4].c_str(), nullptr );
                TerraFloat3 pos = terra_f3_set ( x, y, z );

                if ( !_scene.move_mesh ( args[1].c_str(), pos ) ) {
                    return 1;
                }

                return 0;
            } );
        }

        return 0;
    };
    //
    _c_map[CMD_CLEAR_NAME] = cmd_clear;
    _c_map[CMD_HELP_NAME] = cmd_help;
    _c_map[CMD_LOAD_NAME] = cmd_load;
    _c_map[CMD_STEP_NAME] = cmd_step;
    _c_map[CMD_LOOP_NAME] = cmd_loop;
    _c_map[CMD_PAUSE_NAME] = cmd_pause;
    _c_map[CMD_SAVE_NAME] = cmd_save;
    //_c_map[CMD_TOGGLE_NAME] = cmd_toggle;
    _c_map[CMD_OPTION_NAME] = cmd_option;
    _c_map[CMD_RESIZE_NAME] = cmd_resize;
    _c_map[CMD_HIDE_NAME] = cmd_hide;
    _c_map[CMD_STATS_NAME] = cmd_stats;
    _c_map[CMD_MESH_NAME] = cmd_mesh;
}
#endif

int App::_boot() {
    // Initialize graphics component
    int width = Config::read_i ( Config::RENDER_WIDTH );
    int height = Config::read_i ( Config::RENDER_HEIGHT );
    bool gfx_init = _gfx.init ( width, height, "Satellite",
    [ = ] ( int w, int h ) { // on resize
    },
    [ = ] ( const ImGuiIO & io ) { // on key
    } );

    if ( !gfx_init ) {
        return EXIT_FAILURE;
    }

    Log::flush();

    if ( !Config::load () ) {
        Log::warning ( STR ( "No configuration file found, defaulting all options." ) );
    }

    // Initialize components
    _on_config_change ( true );
    _set_ui();
    _set_renderer(Config::read_s(Config::RENDERER_TYPE));
    _set_camera("");
    _set_camera_controls("");

    // Register input callbacks
    key_receiver = this;
    glfwSetInputMode(_gfx.window_handle(), GLFW_LOCK_KEY_MODS, GLFW_TRUE);
    glfwSetKeyCallback(_gfx.window_handle(), glfw_key_callback);

    // Load scene
    const string scene_path = Config::read_s(Config::RENDER_SCENE_PATH);
    if (!scene_path.empty()) {
        _scene.load(scene_path.c_str());
    }

    return EXIT_SUCCESS;
}

void App::_shutdown() {
    // TODO
}

void App::_register_message_listener() {
    Messenger::register_listener(
        [this](const MessageType type, const MessagePayload& _data) {
            switch (type) {
            case MSG_SET_RENDERER: {
                const auto& data = (const MessageSetRenderer&)_data;
                printf("set renderer type %s \n", data.type.c_str());
                _set_renderer(data.type);
                break;
            }

            case MSG_SET_CAMERA: {
                const auto& data = (const MessageSetCamera&)_data;
                printf("set camera %s\n", data.type.c_str());
                _set_camera(data.type);
                break;
            }
            case MSG_SET_CAMERA_CONTROLS: {
                const auto& data = (const MessageSetCamera&)_data;
                printf("set camera controls %s\n", data.type.c_str());
                _set_camera_controls(data.type);
                break;
            }
            default: assert(false);
            }

        }, {
            MSG_SET_RENDERER,
            MSG_SET_CAMERA,
            MSG_SET_CAMERA_CONTROLS
        });
}

int App::_opt_set ( int opt, const std::string& value ) {
    auto setter = [this, opt, value]() {
        Config::write ( opt, value );
        return 0;
    };
    bool clear = opt >= Config::RENDER_BEGIN && opt <= Config::RENDER_END;
    return _opt_set ( clear, setter );
}

int App::_opt_set ( int opt, int value ) {
    auto setter = [this, opt, value]() {
        Config::write_i ( opt, value );
        return 0;
    };
    bool clear = opt >= Config::RENDER_BEGIN && opt <= Config::RENDER_END;
    return _opt_set ( clear, setter );
}

int App::_opt_set ( bool clear, std::function< int() > setter ) {
    bool result = setter();

    if ( result != 0 ) {
        return result;
    }

    _on_config_change ( clear );
}

void App::_on_config_change ( bool clear ) {
    _scene.update_config();
    _gfx.update_config();
    //_visualizer.update_config();

    if ( clear ) {
        _clear();
    }
}
