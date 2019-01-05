#pragma once

// C++ STL
#include <string>
#include <vector>
#include <memory>

// Terra
#include <Terra.h>

//
// Handles loading of models and materials (using Apollo).
// Keeps track of active scene options and exposes read/write access.
// Config and Scene purposely have two different views of the options.
// Config doesn't know about Terra (e.g. string => enumerator mappings) and stores 'default' options
// Scene keeps track of the current scene options to be used by any render call
// and also validates option values.
// see apply_options_to_config() to flush the latter into the former, which allows you to
// save the current render options and load them when starting the app in the future.
//
class Scene {
  public:
    static TerraTonemappingOperator to_terra_tonemap ( std::string& str );
    static TerraAccelerator         to_terra_accelerator ( std::string& str );
    static TerraSamplingMethod      to_terra_sampling ( std::string& str );
    static const char*              from_terra_tonemap ( TerraTonemappingOperator v );
    static const char*              from_terra_accelerator ( TerraAccelerator v );
    static const char*              from_terra_sampling ( TerraSamplingMethod v );

  public:
    Scene();
    ~Scene();

    // Loads models, materials, textures and initializes acceleration structure
    bool load ( const char* filename );

    // Releases all previously load()ed resources
    void clear();

    // Reset options to default (either default values or loaded from config file).
    void reset_options();

    // Sets the internal scene values as default (Config::write_*)
    // Any subsequent call to reset_options will load the last applied values.
    // **does not** call Config::save()
    void apply_options_to_config();

    // Sets render options that will be used by Terra.
    // opt is one of Config::RENDER_ options
    // Checks integral => floating => string
    bool set_opt ( int opt, const char* value );

    // Dumps all the current option values in Log::info
    void dump_opts();

    // To be called when terra_render() is not executing
    // Returns a TerraScene that can be used for rendering
    HTerraScene construct_terra_scene();

    // Somewhat readable scene name
    const char* name() const;

    // Technically it should either be: (in this order)
    // - default value
    // - autoplacement
    // - imported with scene
    // Currently it stops at the first one (TODO)
    TerraCamera default_camera();

  private:
    bool          _set_opt_safe ( int opt, const void* data );
    TerraTexture* _allocate_texture ( const char* texture );

    std::string       _name;
    TerraCamera       _default_camera;
    HTerraScene       _scene;
    TerraSceneOptions _opts;
    bool              _first_load = true;

    // Hopefully temporary
    std::vector<std::unique_ptr<TerraTexture>> _textures;
};
