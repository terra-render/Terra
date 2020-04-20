#pragma once

// C++ STL
#include <string>
#include <vector>
#include <memory>

// Terra
#include <Terra.h>

#include <Config.hpp>

struct ApolloModel;
struct ApolloMaterial;
struct ApolloTexture;

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
    struct ObjectState {
        const char* name;
        float x, y, z;
    };

  public:
    Scene();
    ~Scene();

    // Loads models, materials, textures and initializes acceleration structure
    bool load ( const char* filename );

    // Releases all previously load()ed resources
    void clear();

    // Call this to notify of changes to config
    void update_config();

    // To be called when terra_render() is not executing
    // Returns a TerraScene that can be used for rendering
    HTerraScene construct_terra_scene();

    // Somewhat readable scene name
    const char* name() const;

    const TerraCamera& get_camera();

    // This invalidates the current scene! Need to call construct_terra_scene() again
    bool move_mesh ( const char* name, const TerraFloat3& new_pos );

    bool mesh_exists ( const char* name );

    void get_mesh_states ( std::vector<ObjectState>& states );

    const TerraSceneOptions& get_options();

  private:
    TerraTexture* _allocate_texture ( const char* texture );
    bool          _load_scene ( const char* filename );
    bool          _build_scene();
    void          _read_config();

    ApolloModel* _apollo_model = NULL;
    ApolloMaterial* _apollo_materials = NULL;
    ApolloTexture* _apollo_textures = NULL;

    std::string       _name;
    std::string       _path;
    TerraCamera       _camera;
    HTerraScene       _scene;
    TerraSceneOptions _opts;
    bool              _first_load = true;

    TerraFloat3       _envmap_color;

    // Hopefully temporary
    std::vector<std::unique_ptr<TerraTexture>> _textures;
    // Move gen (because mesh move is idx driven)
    std::vector<uint32_t> _vert_gens;
    uint32_t _gen = 0;
};
