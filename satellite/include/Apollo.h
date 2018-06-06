#ifndef _APOLLO_H_
#define _APOLLO_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APOLLO_SUCCESS = 0,
    APOLLO_MATERIAL_FILE_NOT_FOUND = 1,
    APOLLO_FILE_NOT_FOUND = 2,
    APOLLO_MATERIAL_NOT_FOUND = 3,
    APOLLO_INVALID_FORMAT = 4,
    APOLLO_ERROR
} ApolloResult;

typedef struct {
    float x;
    float y;
} ApolloFloat2;

bool apollo_equalf2 ( ApolloFloat2* a, ApolloFloat2* b ) {
    return a->x == b->x && a->y == b->y;
}

typedef struct {
    float x;
    float y;
    float z;
} ApolloFloat3;

float apollo_absf ( float f ) {
    return f < 0 ? -f : f;
}

int apollo_equalf3 ( ApolloFloat3* a, ApolloFloat3* b ) {
    return a->x == b->x && a->y == b->y && a->z == b->z;
    //return apollo_absf ( a->x - b->x ) < 0.0001f && apollo_absf ( a->y - b->y ) < 0.0001f && apollo_absf ( a->z - b->z ) < 0.0001f;
}

typedef struct {
    float x;
    float y;
    float z;
    float w;
} ApolloFloat4;

typedef struct {
    ApolloFloat4 r1;
    ApolloFloat4 r2;
    ApolloFloat4 r3;
    ApolloFloat4 r4;
} ApolloMatrix4;

typedef struct {
    ApolloFloat3 min;
    ApolloFloat3 max;
} ApolloAABB;

typedef struct {
    ApolloFloat3 center;
    float radius;
} ApolloSphere;

typedef int ApolloIndex;

typedef struct ApolloVertex {
    ApolloFloat3 pos;
    ApolloFloat2 tex;
    ApolloFloat3 norm;
} ApolloVertex;

typedef struct ApolloMeshFace {
    ApolloIndex a;
    ApolloIndex b;
    ApolloIndex c;
    ApolloFloat3 normal;
    ApolloFloat3 tangent;
    ApolloFloat3 bitangent;
} ApolloMeshFace;

typedef struct ApolloMesh {
    ApolloMeshFace* faces;
    int face_count;
    int material_id;
    ApolloAABB aabb;
    ApolloSphere bounding_sphere;
} ApolloMesh;

typedef struct ApolloModel {
    ApolloVertex* vertices;
    int vertex_count;
    ApolloMesh* meshes;
    int mesh_count;
    ApolloAABB aabb;
    ApolloSphere bounding_sphere;
} ApolloModel;

// ------------------------------------------------------------------------------------------------------
// Materials
// A subset of the mtl obj specification http://paulbourke.net/dataformats/mtl
// It's a little bit weird as the PBR extension does not define an illumination model
// APOLLO_PBR is then returned whenever one of the PBR attributes (from Pr onwards) is encountered
// ------------------------------------------------------------------------------------------------------
typedef enum ApolloBSDF {
    APOLLO_DIFFUSE,
    APOLLO_SPECULAR,
    APOLLO_MIRROR,
    APOLLO_PBR
} ApolloBSDF;

// Path-tracer subset of http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr
typedef struct ApolloMaterial {
    char*        name;
    float        ior; // Ni
    ApolloFloat3 diffuse; // Kd
    int          diffuse_map_id; // map_Kd
    ApolloFloat3 specular; // Ks
    int          specular_map_id; // map_Ks
    float        specular_exp; // Ns [0 1000]
    int          specular_exp_map_id;
    int          bump_map_id; // bump
    int          disp_map_id; // disp
    float        roughness; // Pr
    int          roughness_map_id; // map_Pr
    float        metallic; // Pm
    int          metallic_map_id; // map_Pm
    ApolloFloat3 emissive; // Ke
    int          emissive_map_id; // map_Ke
    int          normal_map_id; // norm
    ApolloBSDF   bsdf;
} ApolloMaterial;

typedef struct ApolloMaterialCollection {
    ApolloMaterial* materials;
    int count;
    int capacity;
} ApolloMaterialCollection;

typedef struct ApolloTexture {
    char name[256];
} ApolloTexture;

typedef struct ApolloTextureCollection {
    ApolloTexture* textures;
    int count;
    int capacity;
} ApolloTextureCollection;

void apollo_create_material_collection ( ApolloMaterialCollection* collection );
void apollo_create_texture_collection ( ApolloTextureCollection* collection );
int  apollo_import_model_obj ( const char* filename, ApolloModel* model,
                               ApolloMaterialCollection* materials, ApolloTextureCollection* textures,
                               int right_handed_coords, int flip_faces );
//int apollo_export_model_obj(const char* filename, ApolloModel* model);

//int apollo_open_material_lib(const char* filename, ApolloMaterialLib* lib);
//int apollo_export_material_lib(const char* filename, ApolloMaterialLib* lib);

#ifndef APOLLO_LOG_ERR
    #define APOLLO_LOG_ERR(fmt, ...) fprintf(stderr, "Apollo Error: " fmt "\n", __VA_ARGS__)
#endif

#ifndef APOLLO_LOG_WARN
    #define APOLLO_LOG_WARN(fmt, ...) fprintf(stdout, "Apollo Warning: " fmt "\n", __VA_ARGS__)
#endif

#if defined APOLLO_IMPLEMENTATION

#define APOLLO_MAX_MATERIALS 128

#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct ApolloMaterialLib {
    FILE* file;
    char* filename;
    ApolloMaterial* materials;
    int materials_count;
} ApolloMaterialLib;

void apollo_create_material_collection ( ApolloMaterialCollection* collection ) {
    collection->capacity = 32;
    collection->count = 0;
    collection->materials = ( ApolloMaterial* ) malloc ( sizeof ( *collection->materials ) * collection->capacity );
}

void apollo_create_texture_collection ( ApolloTextureCollection* collection ) {
    collection->capacity = 32;
    collection->count = 0;
    collection->textures = ( ApolloTexture* ) malloc ( sizeof ( *collection->textures ) * collection->capacity );
}

bool apollo_read_float3 ( FILE* file, ApolloFloat3* val ) {
    float x, y, z;

    if ( fscanf ( file, "%f %f %f", &x, &y, &z ) != 3 ) {
        return false;
    }

    val->x = x;
    val->y = y;
    val->z = z;
    return true;
}

void apollo_texture_collection_insert ( ApolloTextureCollection* collection, const ApolloTexture* texture ) {
    if ( collection->count == collection->capacity ) {
        ApolloTexture* new_texs = ( ApolloTexture* ) malloc ( sizeof ( ApolloTexture ) * collection->capacity * 2 );
        memcpy ( new_texs, collection->textures, collection->count );
        free ( collection->textures );
        collection->textures = new_texs;
    }

    collection->textures[collection->count++] = *texture;
}

int apollo_texture_collection_find ( const ApolloTextureCollection* textures, const char* texture_name ) {
    for ( int i = 0; i < textures->count; ++i ) {
        if ( strcmp ( texture_name, textures->textures[i].name ) == 0 ) {
            return i;
        }
    }

    return -1;
}


// TODO: The overall error reporting should be improved, once finer grained
// APOLLO_ERRORs are spread around they should also be moved here.
// >= 0 valid texture index
// -1 -> format error
// -2 -> texture error
int apollo_read_texture ( FILE* file, ApolloTextureCollection* textures ) {
    char map_name[256];

    if ( fscanf ( file, "%s", map_name ) != 1 ) {
        return -1;
    }

    int idx = apollo_texture_collection_find ( textures, map_name );

    if ( idx == -1 ) {
        ApolloTexture texture;
        strncpy ( texture.name, map_name, 256 );
        idx = textures->count;
        apollo_texture_collection_insert ( textures, &texture );
    }

    return idx;
}

int apollo_open_material_lib ( const char* filename, ApolloMaterialLib* lib, ApolloTextureCollection* textures ) {
    FILE* file = NULL;
    file = fopen ( filename, "r" );

    if ( file == NULL ) {
        APOLLO_LOG_ERR ( "Cannot open file %s\n", filename );
        return APOLLO_ERROR;
    }

    char key[32];
    int key_len = 0;
    int materials_count = 0;
    ApolloMaterial* materials = ( ApolloMaterial* ) malloc ( sizeof ( *materials ) * APOLLO_MAX_MATERIALS );

    for ( int i = 0; i < APOLLO_MAX_MATERIALS; ++i ) {
        materials[i].bump_map_id = -1;
        materials[i].diffuse_map_id = -1;
        materials[i].disp_map_id = -1;
        materials[i].emissive_map_id = -1;
        materials[i].metallic_map_id = -1;
        materials[i].normal_map_id = -1;
        materials[i].roughness_map_id = -1;
        materials[i].specular_map_id = -1;
        materials[i].specular_exp_map_id = -1;
    }

    while ( fscanf ( file, "%s", key ) != EOF ) {
        // New material tag
        if ( strcmp ( key, "mat" ) == 0 || strcmp ( key, "newmtl" ) == 0 ) {
            char mat_name[32];

            if ( fscanf ( file, "%s", mat_name ) != 1 ) {
                APOLLO_LOG_ERR ( "Read error on file %s\n", filename );
                goto error;
            }

            for ( int i = 0; i < materials_count; ++i ) {
                if ( strcmp ( mat_name, materials[i].name ) == 0 ) {
                    APOLLO_LOG_ERR ( "Duplicate material name (%s) in materials library %s\n", mat_name, filename );
                    goto error;
                }
            }

            int mat_name_len = ( int ) strlen ( mat_name );
            materials[materials_count].name = ( char* ) malloc ( sizeof ( char ) * ( mat_name_len + 1 ) );
            strcpy ( materials[materials_count].name, mat_name );
            ++materials_count;
        } else if ( strcmp ( key, "Ni" ) == 0 ) {
            float Ni;

            if ( fscanf ( file, "%f", &Ni ) == 0 ) {
                APOLLO_LOG_ERR ( "Format error reading Ni on file %s\n", filename );
                goto error;
            }
        }
        // Diffuse
        else if ( strcmp ( key, "Kd" ) == 0 ) {
            ApolloFloat3 Kd;

            if ( !apollo_read_float3 ( file, &Kd ) ) {
                APOLLO_LOG_ERR ( "Format error reading Kd on file %s\n", filename );
                goto error;
            }

            materials[materials_count - 1].diffuse = Kd;
        } else if ( strcmp ( key, "map_Kd" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].diffuse_map_id = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading diffuse texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Specular
        else if ( strcmp ( key, "Ks" ) == 0 ) {
            ApolloFloat3 Kd;

            if ( !apollo_read_float3 ( file, &Kd ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ks on file %s\n", filename );
                goto error;
            }

            materials[materials_count - 1].specular = Kd;
        } else if ( strcmp ( key, "map_Ks" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].specular_map_id = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading specular texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Specular exponent
        else if ( strcmp ( key, "Ns" ) == 0 ) {
            float Ns;

            if ( fscanf ( file, "%f", &Ns ) != 1 ) {
                APOLLO_LOG_ERR ( "Format error reading Ns on file %s\n", filename );
                goto error;
            }

            materials[materials_count - 1].specular_exp = Ns;
        }
        // Bump mapping
        else if ( strcmp ( key, "bump" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].bump_map_id = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading bump texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Roughness
        else if ( strcmp ( key, "Pr" ) == 0 ) {
            float Pr;

            if ( fscanf ( file, "%f", &Pr ) != 1 ) {
                APOLLO_LOG_ERR ( "Format error reading Pr on file %s\n", filename );
                goto error;
            }

            materials[materials_count - 1].roughness = Pr;
            materials[materials_count - 1].bsdf = APOLLO_PBR;
        } else if ( strcmp ( key, "map_Pr" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].roughness_map_id = idx;
                materials[materials_count - 1].bsdf = APOLLO_PBR;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading roughness texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Metallic
        else if ( strcmp ( key, "Pm" ) == 0 ) {
            float Pm;

            if ( fscanf ( file, "%f", &Pm ) != 1 ) {
                APOLLO_LOG_ERR ( "Format error reading Pm on file %s\n", filename );
                goto error;
            }

            materials[materials_count - 1].metallic = Pm;
            materials[materials_count - 1].bsdf = APOLLO_PBR;
        } else if ( strcmp ( key, "map_Pm" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].metallic_map_id = idx;
                materials[materials_count - 1].bsdf = APOLLO_PBR;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading metallic texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Emissive
        else if ( strcmp ( key, "Ke" ) == 0 ) {
            ApolloFloat3 Ke;

            if ( !apollo_read_float3 ( file, &Ke ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ke on file %s\n", filename );
                goto error;
            }

            materials[materials_count - 1].emissive = Ke;
            materials[materials_count - 1].bsdf = APOLLO_PBR;
        } else if ( strcmp ( key, "map_Ke" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].emissive_map_id = idx;
                materials[materials_count - 1].bsdf = APOLLO_PBR;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading emissive texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Normal map
        else if ( strcmp ( key, "normal" ) == 0 ) {
            int idx = apollo_read_texture ( file, textures );

            if ( idx >= 0 ) {
                materials[materials_count - 1].normal_map_id = idx;
                materials[materials_count - 1].bsdf = APOLLO_PBR;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading normal texture on file %s\n", idx, filename );
                goto error;
            }
        } else if ( strcmp ( key, "d" ) == 0 || strcmp ( key, "Tr" ) == 0 || strcmp ( key, "Tf" ) == 0 || strcmp ( key, "Ka" ) == 0 ) {
            while ( getc ( file ) != '\n' );

            // ...
        } else if ( strcmp ( key, "illum" ) == 0 ) {
            if ( materials[materials_count - 1].bsdf == APOLLO_PBR ) {
                continue;
            }

            int illum;

            if ( fscanf ( file, "%d", &illum ) != 1 ) {
                APOLLO_LOG_ERR ( "Failed to read illum on file %s", filename );
                goto error;
            }

            // Apollo pbr is handled at the end
            switch ( illum ) {
                case 2:
                    materials[materials_count - 1].bsdf = APOLLO_SPECULAR;
                    break;

                case 5:
                    materials[materials_count - 1].bsdf = APOLLO_MIRROR;
                    break;

                default:
                    materials[materials_count - 1].bsdf = APOLLO_DIFFUSE;
                    break;
            }
        } else {
            APOLLO_LOG_ERR ( "Unexpected field name %s in materials file %s\n", key, filename );
            goto error;
        }
    }

    lib->file = file;
    lib->filename = ( char* ) malloc ( sizeof ( char ) * ( strlen ( filename ) + 1 ) );
    strcpy ( lib->filename, filename );
    lib->materials = ( ApolloMaterial* ) malloc ( sizeof ( *lib->materials ) * materials_count );

    for ( int i = 0; i < materials_count; ++i ) {
        lib->materials[i] = materials[i];
    }

    lib->materials_count = materials_count;
    free ( materials );
    return APOLLO_SUCCESS;
error:
    free ( materials );
    return APOLLO_ERROR;
}

int apollo_import_model_obj ( const char* filename, ApolloModel* model, ApolloMaterialCollection* materials, ApolloTextureCollection* textures, int rh, int flip_faces ) {
    FILE* file = NULL;
    file = fopen ( filename, "r" );

    if ( file == NULL ) {
        fprintf ( stderr, "Cannot open file %s", filename );
        return APOLLO_FILE_NOT_FOUND;
    }

    int filename_len = ( int ) strlen ( filename );
    const char* dir_end = filename + filename_len;

    while ( --dir_end != filename && *dir_end != '/' && *dir_end != '\\' )
        ;

    assert ( filename_len < 1024 );
    char base_dir[1024];
    base_dir[0] = '\0';
    size_t base_dir_len = 0;

    if ( dir_end != filename ) {
        ++dir_end;
        base_dir_len = dir_end - filename;
        strncpy ( base_dir, filename, base_dir_len );
    }

    ApolloFloat3* vertex_pos = ( ApolloFloat3* ) malloc ( sizeof ( *vertex_pos ) * 300000 );
    int pos_count = 0;
    ApolloFloat2* tex_coords = ( ApolloFloat2* ) malloc ( sizeof ( *tex_coords ) * 300000 );
    int tex_count = 0;
    ApolloFloat3* normals = ( ApolloFloat3* ) malloc ( sizeof ( *normals ) * 300000 );
    int normal_count = 0;
    ApolloIndex* indices = ( ApolloIndex* ) malloc ( sizeof ( *indices ) * 300000 );
    int index_count = 0;
    ApolloVertex* vertices = ( ApolloVertex* ) malloc ( sizeof ( *vertices ) * 300000 );
    int vertex_count = 0;
    int* mesh_offsets = ( int* ) malloc ( sizeof ( *mesh_offsets ) * 300000 );
    int mesh_count = 0;
    int face_count = 0;
    int* mesh_smoothing_group = ( int* ) malloc ( sizeof ( *mesh_smoothing_group ) * 1280 );

    for ( int i = 0; i < 1280; ++i ) {
        mesh_smoothing_group[i] = 0;
    }

    //vv
    ApolloMaterial* used_materials = ( ApolloMaterial* ) malloc ( sizeof ( *used_materials ) * 1280 );
    int used_materials_count = 0;
    ApolloMaterialLib* material_libs = ( ApolloMaterialLib* ) malloc ( sizeof ( *material_libs ) * 128 );
    int material_libs_count = 0;
    int* mesh_material_idxs = ( int* ) malloc ( sizeof ( *mesh_material_idxs ) * 1280 );

    for ( int i = 0; i < 1280; ++i ) {
        mesh_material_idxs[i] = -1;
    }

    //^^
    int x;
    bool material_loaded = false;

    while ( ( x = fgetc ( file ) ) != EOF ) {
        switch ( x ) {
            case '#': // Comment. Skip the line.
            case 'm': {
                if ( material_loaded ) {
                    while ( fgetc ( file ) != '\n' )
                        ;

                    break;
                }

                char key[32];
                fscanf ( file, "%s", key );

                if ( strcmp ( key, "tllib" ) != 0 ) {
                    break;
                }

                char mat_filename[1024];

                if ( base_dir_len > 0 ) {
                    memcpy ( mat_filename, base_dir, base_dir_len );
                }

                fscanf ( file, "%s", mat_filename + base_dir_len );
                int mat_filename_len = ( int ) strlen ( mat_filename );
                int found = 0;

                for ( int i = 0; i < material_libs_count; ++i ) {
                    if ( strcmp ( material_libs[i].filename, mat_filename ) == 0 ) {
                        found = 1;
                        break;
                    }
                }

                if ( apollo_open_material_lib ( mat_filename, &material_libs[material_libs_count++], textures ) != APOLLO_SUCCESS ) {
                    return APOLLO_ERROR;    // TODO free
                }

                material_loaded = true;
                break;
            }

            case 'u': {
                char key[32];
                fscanf ( file, "%s", key );

                if ( strcmp ( key, "semtl" ) != 0 ) {
                    break;
                }

                char mat_name[32];
                fscanf ( file, "%s", mat_name );
                int found = 0;

                for ( int i = 0; i < materials->count; ++i ) {
                    if ( strcmp ( materials->materials[i].name, mat_name ) == 0 ) {
                        mesh_material_idxs[mesh_count - 1] = i;
                        found = 1;
                        break;
                    }
                }

                if ( found ) {
                    break;
                }

                for ( int i = 0; i < used_materials_count; ++i ) {
                    if ( strcmp ( used_materials[i].name, mat_name ) == 0 ) {
                        mesh_material_idxs[mesh_count - 1] = i + materials->count;
                        found = 1;
                        break;
                    }
                }

                if ( found ) {
                    break;
                }

                for ( int i = 0; i < material_libs_count; ++i ) {
                    for ( int j = 0; j < material_libs[i].materials_count; ++j ) {
                        if ( strcmp ( material_libs[i].materials[j].name, mat_name ) == 0 ) {
                            used_materials[used_materials_count] = material_libs[i].materials[j];
                            mesh_material_idxs[mesh_count - 1] = used_materials_count + materials->count;
                            ++used_materials_count;
                            found = 1;
                            break;
                        }
                    }
                }

                if ( !found ) {
                    APOLLO_LOG_ERR ( "Failed to find material %s @ %s", mat_name, filename );
                    return APOLLO_MATERIAL_NOT_FOUND; // TODO free
                }
            }
            break;

            case 'v': {
                // Vertex data.
                x = fgetc ( file );

                switch ( x ) {
                    case ' ': {
                        // Vertex position.
                        ApolloFloat3 pos;
                        fscanf ( file, "%f", &pos.x );
                        fscanf ( file, "%f", &pos.y );
                        fscanf ( file, "%f", &pos.z );

                        if ( rh ) {
                            pos.z = pos.z * -1.f;
                        }

                        vertex_pos[pos_count++] = pos;
                    }
                    break;

                    case 't': {
                        // Vertex UV coords.
                        ApolloFloat2 uv;
                        fscanf ( file, "%f", &uv.x );
                        fscanf ( file, "%f", &uv.y );

                        if ( rh ) {
                            uv.y = 1.f - uv.y;
                        }

                        tex_coords[tex_count++] = uv;
                    }
                    break;

                    case 'n': {
                        ApolloFloat3 norm;
                        fscanf ( file, "%f", &norm.x );
                        fscanf ( file, "%f", &norm.y );
                        fscanf ( file, "%f", &norm.z );

                        if ( rh ) {
                            norm.z = norm.z * -1;
                        }

                        normals[normal_count++] = norm;
                    }
                    break;
                }
            }
            break;

            case 'o':
            case 'g':
                // New mesh
                x = fgetc ( file );

                switch ( x ) {
                    case ' ':
                        mesh_offsets[mesh_count++] = index_count;
                        break;
                }

                while ( fgetc ( file ) != '\n' )
                    ;

                break;

            case 's':
                x = fgetc ( file );
                x = fgetc ( file );

                if ( x == '1' ) {
                    mesh_smoothing_group[mesh_count - 1] = 1;
                } else {
                    mesh_smoothing_group[mesh_count - 1] = 0;

                    while ( fgetc ( file ) != '\n' )
                        ;
                }

                break;

            case 'f':
                // Face definition. Basically a tuple of indices that make up a single face (triangle).
                x = fgetc ( file );

                if ( x != ' ' ) {
                    APOLLO_LOG_ERR ( "Expected space after 'f', but found %c", x );
                    return APOLLO_INVALID_FORMAT;
                }

                {
                    // We are about to process the entire face.
                    // We read the entire face line from the file and store it.
                    // We then prepare to go through the face string, storing and processing one vertex at a time.
                    char face_str[128];
                    fgets ( face_str, sizeof ( face_str ), file );
                    int face_len = ( int ) strlen ( face_str );
                    int face_vertex_count = 0;    // To increase on read from face
                    int offset = 0;         // To increase on read from face
                    char vertex_str[32];
                    int vertex_len = 0;     // To set on read from face
                    int pos_index = -1;
                    int tex_index = -1;
                    int norm_index = -1;
                    int first_vertex_index = -1;
                    int last_vertex_index = -1;

                    // Foreach vertex in face
                    // We assume there are at least 3 vertices per face (seems fair)
                    //while (face_vertex_count < 3) {
                    while ( offset < face_len ) {
                        sscanf ( face_str + offset, "%s", vertex_str );
                        // A new vertex string has been extracted from the face string.
                        // We need to do one more xformation: divide the vertex string into the data pieces the vertex is composed of.
                        // We prepare to go through the vertex string, storing and processing one data piece at a time.
                        vertex_len = ( int ) strlen ( vertex_str );
                        offset += vertex_len + 1;
                        char data_piece[10];
                        int data_piece_len = 0;     // To reset after each processed data piece
                        int data_piece_index = 0;   // To increase after each processed data piece

                        // If the face is not a triangle, we need to split it up into triangles.
                        // To do so, we always use the first and last vetices, plus the one we're currently processing.
                        // The first and last vertex indices will be set later.
                        if ( face_vertex_count >= 3 ) {
                            indices[index_count++] = first_vertex_index;
                            indices[index_count++] = last_vertex_index;
                        }

                        // Foreach data piece in vertex
                        {
                            int i = 0;

                            do {
                                if ( vertex_str[i] != '/' && vertex_str[i] != '\0' ) {
                                    data_piece[data_piece_len++] = vertex_str[i];
                                } else {
                                    if ( data_piece_len > 0 ) {
                                        // Read the data as integer
                                        data_piece[data_piece_len] = '\0';
                                        int data = atoi ( data_piece );

                                        // A new data piece has been extraced, we store it.
                                        switch ( data_piece_index ) {
                                            case 0:
                                                pos_index = data - 1;
                                                break;

                                            case 1:
                                                tex_index = data - 1;
                                                break;

                                            case 2:
                                                norm_index = data - 1;
                                                break;
                                        }

                                        data_piece_len = 0;
                                    }

                                    ++data_piece_index;
                                }
                            } while ( vertex_str[i++] != '\0' );
                        }

                        // In an OBJ file, the faces mark the end of a face.
                        // However it could be that the first face was implicitly started (not listed in the file), so we consider that case,
                        if ( mesh_count == 0 ) {
                            mesh_offsets[mesh_count++] = index_count;
                        }

                        // Finalize the vertex, testing for overlaps if the mesh is smooth and updating the first/last face vertex indices
                        int duplicate = 0;

                        if ( mesh_smoothing_group[mesh_count - 1] == 1 && vertex_count >= 3 ) {
                            for ( int i = 0; i < vertex_count && !duplicate; ++i ) {
                                if ( apollo_equalf3 ( &vertex_pos[pos_index], &vertices[i].pos )
                                        && apollo_equalf2 ( &tex_coords[tex_index], &vertices[i].tex )
                                        /*&& norm_index == vertices[i].norm*/ ) {
                                    indices[index_count] = i;
                                    duplicate = 1;
                                }
                            }
                        }

                        // If it's not a duplicate, add it as a new vertex
                        if ( !duplicate ) {
                            ApolloVertex vertex;
                            vertex.pos = vertex_pos[pos_index];
                            vertex.tex = tex_coords[tex_index];
                            vertex.norm = normals[norm_index];
                            vertices[vertex_count] = vertex;
                            indices[index_count] = vertex_count++;
                        }

                        // Store the first and last vertex index for later
                        if ( first_vertex_index == -1 && face_vertex_count == 0 ) {
                            first_vertex_index = indices[index_count];
                        }

                        if ( face_vertex_count >= 2 ) {
                            last_vertex_index = indices[index_count];
                        }

                        ++face_vertex_count;
                        ++index_count;

                        if ( face_vertex_count >= 3 ) {
                            ++face_count;
                        }
                    }
                }

                break;
        }
    }

    // End of file
    // Add the index count as last offsets in order to make some later stuff easier
    mesh_offsets[mesh_count] = index_count;

    // Flip faces?
    if ( flip_faces ) {
        for ( int i = 0; i < index_count; i += 3 ) {
            ApolloIndex idx = indices[i];
            indices[i] = indices[i + 2];
            indices[i + 2] = idx;
        }
    }

    ApolloFloat3* face_normals = ( ApolloFloat3* ) malloc ( sizeof ( *face_normals ) * face_count );
    ApolloFloat3* face_tangents = ( ApolloFloat3* ) malloc ( sizeof ( *face_tangents ) * face_count );
    ApolloFloat3* face_bitangents = ( ApolloFloat3* ) malloc ( sizeof ( *face_bitangents ) * face_count );

    // Compute face normals
    for ( int i = 0; i < face_count; ++i ) {
        ApolloFloat3 v0 = vertices[indices[i * 3]].pos;
        ApolloFloat3 v1 = vertices[indices[i * 3 + 1]].pos;
        ApolloFloat3 v2 = vertices[indices[i * 3 + 2]].pos;
        // Compute two edges of the triangle in order to be able to compute the normal as the cross prod of the edges.
        ApolloFloat3 e01;
        e01.x = v1.x - v0.x;
        e01.y = v1.y - v0.y;
        e01.z = v1.z - v0.z;
        ApolloFloat3 e02;
        e02.x = v2.x - v0.x;
        e02.y = v2.y - v0.y;
        e02.z = v2.z - v0.z;
        // x prod
        face_normals[i].x = e01.y * e02.z - e01.z * e02.y;
        face_normals[i].y = e01.z * e02.x - e01.x * e02.z;
        face_normals[i].z = e01.x * e02.y - e01.y * e02.x;
        // normalize
        float len = sqrtf ( face_normals[i].x * face_normals[i].x + face_normals[i].y * face_normals[i].y + face_normals[i].z * face_normals[i].z );
        face_normals[i].x /= len;
        face_normals[i].y /= len;
        face_normals[i].z /= len;
    }

    // Recompute the vertex normals for smooth meshes
    for ( int m = 0; m < mesh_count; ++m ) {
        int mesh_index_base = mesh_offsets[m];
        int mesh_index_count = mesh_offsets[m + 1] - mesh_index_base;
        int mesh_face_count = mesh_index_count / 3;

        if ( mesh_index_count % 3 != 0 ) {
            APOLLO_LOG_ERR ( "Malformed .obj file @ %s", filename );
            return APOLLO_INVALID_FORMAT;
        }

        // If the mesh isn't smooth, skip it
        if ( mesh_smoothing_group[m] == 0 ) {
            for ( int i = 0; i < mesh_face_count; ++i ) {
                vertices[indices[mesh_index_base + i * 3]].norm = face_normals[mesh_index_base / 3 + i];
                vertices[indices[mesh_index_base + i * 3 + 1]].norm = face_normals[mesh_index_base / 3 + i];
                vertices[indices[mesh_index_base + i * 3 + 2]].norm = face_normals[mesh_index_base / 3 + i];
            }

            continue;
        }

        // Vertex normals foreach mesh
        int* computed_indices = ( int* ) malloc ( sizeof ( *computed_indices ) * mesh_index_count );
        int computed_indices_count = 0;

        for ( int i = 0; i < mesh_index_count; ++i ) {
            ApolloFloat3 norm_sum;
            norm_sum.x = norm_sum.y = norm_sum.z = 0;
            // skip the index if the corresponding vertex has already been processed
            int already_computed_vertex = 0;

            for ( int j = 0; j < computed_indices_count; ++j ) {
                if ( computed_indices[j] == indices[mesh_index_base + i] ) {
                    already_computed_vertex = 1;
                    break;
                }
            }

            if ( already_computed_vertex ) {
                continue;
            } else {
                computed_indices[computed_indices_count++] = indices[mesh_index_base + i];
            }

            // Foreach vertex we search for its one-ring:
            // We go through all triangles and check if one of their vertices is this one (the i-th).
            // If it is, that face's normal takes part into the computation.
            for ( int j = 0; j < mesh_face_count; ++j ) {
                if ( indices[mesh_index_base + j * 3] == indices[mesh_index_base + i]
                        || indices[mesh_index_base + j * 3 + 1] == indices[mesh_index_base + i]
                        || indices[mesh_index_base + j * 3 + 2] == indices[mesh_index_base + i] ) {
                    norm_sum.x += face_normals[mesh_index_base / 3 + j].x;
                    norm_sum.y += face_normals[mesh_index_base / 3 + j].y;
                    norm_sum.z += face_normals[mesh_index_base / 3 + j].z;
                }
            }

            ApolloFloat3 normal;
            normal.x = norm_sum.x;// / face_count;
            normal.y = norm_sum.y;// / face_count;
            normal.z = norm_sum.z;// / face_count;
            float len = sqrtf ( normal.x * normal.x + normal.y * normal.y + normal.z * normal.z );
            normal.x /= len;
            normal.y /= len;
            normal.z /= len;
            vertices[indices[mesh_index_base + i]].norm = normal;
        }

        free ( computed_indices );
    }

    // Compute tangents and bitangents
    for ( int i = 0; i < face_count; ++i ) {
        if ( face_normals[i].x * face_normals[i].x > face_normals[i].y * face_normals[i].y ) {
            float f = sqrtf ( face_normals[i].x * face_normals[i].x + face_normals[i].z * face_normals[i].z );
            face_tangents[i].x = face_normals[i].z * f;
            face_tangents[i].y = 0;
            face_tangents[i].z = -face_normals[i].x * f;
        } else {
            float f = sqrtf ( face_normals[i].y * face_normals[i].y + face_normals[i].z * face_normals[i].z );
            face_tangents[i].x = 0;
            face_tangents[i].y = -face_normals[i].z * f;
            face_tangents[i].z = face_normals[i].y * f;
        }

        // x prod
        face_bitangents[i].x = face_normals[i].y * face_tangents[i].z - face_normals[i].z * face_tangents[i].y;
        face_bitangents[i].y = face_normals[i].z * face_tangents[i].x - face_normals[i].x * face_tangents[i].z;
        face_bitangents[i].z = face_normals[i].x * face_tangents[i].y - face_normals[i].y * face_tangents[i].x;
    }

    // Let's fill up the MaterialCollection
    if ( materials->capacity - materials->count < used_materials_count ) {
        ApolloMaterial* new_mats = ( ApolloMaterial* ) malloc ( sizeof ( *new_mats ) * ( used_materials_count + materials->count ) );
        free ( materials->materials );
        materials->materials = new_mats;
    }

    for ( int i = 0; i < used_materials_count; ++i ) {
        materials->materials[i + materials->count] = used_materials[i];
    }

    materials->count += used_materials_count;
    // Let's build the ApolloModel
    // Vertices
    model->vertices = ( ApolloVertex* ) malloc ( sizeof ( ApolloVertex ) * vertex_count );
    model->vertex_count = vertex_count;

    for ( int i = 0; i < vertex_count; ++i ) {
        model->vertices[i].pos = vertices[i].pos;
        model->vertices[i].tex = vertices[i].tex;
        model->vertices[i].norm = vertices[i].norm;
    }

    // Meshes
    model->meshes = ( ApolloMesh* ) malloc ( sizeof ( *model->meshes ) * mesh_count );
    model->mesh_count = mesh_count;

    for ( int i = 0; i < mesh_count; ++i ) {
        // Faces
        int mesh_index_base = mesh_offsets[i];
        int mesh_index_count = mesh_offsets[i + 1] - mesh_index_base;
        int mesh_face_count = mesh_index_count / 3;

        if ( mesh_index_count % 3 != 0 ) {
            APOLLO_LOG_ERR ( "Malformed mesh file @ %s", filename );
            return APOLLO_INVALID_FORMAT;
        }

        model->meshes[i].faces = ( ApolloMeshFace* ) malloc ( sizeof ( *model->meshes[i].faces ) * mesh_face_count );
        model->meshes[i].face_count = mesh_face_count;

        for ( int j = 0; j < mesh_face_count; ++j ) {
            model->meshes[i].faces[j].a = indices[mesh_index_base + j * 3];
            model->meshes[i].faces[j].b = indices[mesh_index_base + j * 3 + 1];
            model->meshes[i].faces[j].c = indices[mesh_index_base + j * 3 + 2];
            model->meshes[i].faces[j].normal = face_normals[mesh_index_base / 3 + j];
            model->meshes[i].faces[j].tangent = face_tangents[mesh_index_base / 3 + j];
            model->meshes[i].faces[j].bitangent = face_bitangents[mesh_index_base / 3 + j];
        }

        // Material
        model->meshes[i].material_id = mesh_material_idxs[i];
        // AABB
        ApolloFloat3 min = vertices[indices[mesh_index_base]].pos;
        ApolloFloat3 max = vertices[indices[mesh_index_base]].pos;

        for ( int j = 1; j < mesh_index_count; ++j ) {
            ApolloFloat3 pos = vertices[indices[mesh_index_base + j]].pos;

            if ( pos.x < min.x ) {
                min.x = pos.x;
            }

            if ( pos.y < min.y ) {
                min.y = pos.y;
            }

            if ( pos.z < min.z ) {
                min.z = pos.z;
            }

            if ( pos.x > max.x ) {
                max.x = pos.x;
            }

            if ( pos.y > max.y ) {
                max.y = pos.y;
            }

            if ( pos.z > max.z ) {
                max.z = pos.z;
            }
        }

        model->meshes[i].aabb.min = min;
        model->meshes[i].aabb.max = max;
        // Sphere
        ApolloFloat3 center;
        center.x = ( max.x + min.x ) / 2.f;
        center.y = ( max.y + min.y ) / 2.f;
        center.z = ( max.z + min.z ) / 2.f;
        float max_dist = 0;

        for ( int j = 1; j < mesh_index_count; ++j ) {
            ApolloFloat3 pos = vertices[indices[mesh_index_base + j]].pos;
            ApolloFloat3 center_to_pos;
            center_to_pos.x = pos.x - center.x;
            center_to_pos.y = pos.y - center.y;
            center_to_pos.z = pos.z - center.z;
            float dist = ( float ) sqrt ( center_to_pos.x * center_to_pos.x + center_to_pos.y * center_to_pos.y + center_to_pos.z * center_to_pos.z );

            if ( dist > max_dist ) {
                max_dist = dist;
            }
        }

        model->meshes[i].bounding_sphere.center.x = center.x;
        model->meshes[i].bounding_sphere.center.y = center.y;
        model->meshes[i].bounding_sphere.center.z = center.z;
        model->meshes[i].bounding_sphere.radius = max_dist;
    }

    // AABB
    ApolloFloat3 min = vertices[0].pos;
    ApolloFloat3 max = vertices[0].pos;

    for ( int j = 1; j < vertex_count; ++j ) {
        ApolloFloat3 pos = vertices[j].pos;

        if ( pos.x < min.x ) {
            min.x = pos.x;
        }

        if ( pos.y < min.y ) {
            min.y = pos.y;
        }

        if ( pos.z < min.z ) {
            min.z = pos.z;
        }

        if ( pos.x > max.x ) {
            max.x = pos.x;
        }

        if ( pos.y > max.y ) {
            max.y = pos.y;
        }

        if ( pos.z > max.z ) {
            max.z = pos.z;
        }
    }

    model->aabb.min = min;
    model->aabb.max = max;
    // Sphere
    ApolloFloat3 center;
    center.x = ( max.x + min.x ) / 2.f;
    center.y = ( max.y + min.y ) / 2.f;
    center.z = ( max.z + min.z ) / 2.f;
    float max_dist = 0;

    for ( int j = 1; j < vertex_count; ++j ) {
        ApolloFloat3 pos = vertices[j].pos;
        ApolloFloat3 center_to_pos;
        center_to_pos.x = pos.x - center.x;
        center_to_pos.y = pos.y - center.y;
        center_to_pos.z = pos.z - center.z;
        float dist = ( float ) sqrt ( center_to_pos.x * center_to_pos.x + center_to_pos.y * center_to_pos.y + center_to_pos.z * center_to_pos.z );

        if ( dist > max_dist ) {
            max_dist = dist;
        }
    }

    model->bounding_sphere.center.x = center.x;
    model->bounding_sphere.center.y = center.y;
    model->bounding_sphere.center.z = center.z;
    model->bounding_sphere.radius = max_dist;
    fclose ( file );

    for ( int i = 0; i < material_libs_count; ++i ) {
        fclose ( material_libs[i].file );
    }

    free ( used_materials );
    free ( material_libs );
    free ( mesh_material_idxs );
    free ( vertex_pos );
    free ( tex_coords );
    free ( normals );
    free ( indices );
    free ( vertices );
    free ( mesh_offsets );
    return APOLLO_SUCCESS;
}

#if 0
int apollo_export_model_obj ( const char* filename, ApolloModel* model ) {
    const char* pre = "apollodump_";
    int pre_len = strlen ( pre );
    int name_len = strlen ( filename );
    char* dump_filename = malloc ( sizeof ( char ) * ( pre_len + name_len + 1 ) );
    strcpy ( dump_filename, pre );
    strcpy ( dump_filename + pre_len, filename );
    file = fopen ( dump_filename, "w" );

    for ( int i = 0; i < pos_count; ++i ) {
        fprintf ( file, "v %f %f %f\n", vertex_pos[i].x, vertex_pos[i].y, vertex_pos[i].z );
    }

    for ( int i = 0; i < tex_count; ++i ) {
        fprintf ( file, "vt %f %f\n", tex_coords[i].x, tex_coords[i].y );
    }

    for ( int i = 0; i < normal_count; ++i ) {
        fprintf ( file, "vn %f %f %f\n", normals[i].x, normals[i].y, normals[i].z );
    }

    for ( int i = 0; i < face_count; ++i ) {
        fprintf ( file, "xx %f %f %f\n", face_normals[i].x, face_normals[i].y, face_normals[i].z );
    }

    for ( int i = 0; i < index_count; i += 3 ) {
        fprintf ( file, "f " );

        // Vertex 1
        if ( vertices[indices[i]].pos != -1 ) {
            fprintf ( file, "%d", vertices[indices[i]].pos + 1 );
        }

        fprintf ( file, "/" );

        if ( vertices[indices[i]].tex != -1 ) {
            fprintf ( file, "%d", vertices[indices[i]].tex + 1 );
        }

        fprintf ( file, "/" );

        if ( vertices[indices[i]].norm != -1 ) {
            fprintf ( file, "%d", vertices[indices[i]].norm + 1 );
        }

        fprintf ( file, " " );

        // Vertex 2
        if ( vertices[indices[i + 1]].pos != -1 ) {
            fprintf ( file, "%d", vertices[indices[i + 1]].pos + 1 );
        }

        fprintf ( file, "/" );

        if ( vertices[indices[i + 1]].tex != -1 ) {
            fprintf ( file, "%d", vertices[indices[i + 1]].tex + 1 );
        }

        fprintf ( file, "/" );

        if ( vertices[indices[i + 1]].norm != -1 ) {
            fprintf ( file, "%d", vertices[indices[i + 1]].norm + 1 );
        }

        fprintf ( file, " " );

        // Vertex 3
        if ( vertices[indices[i + 2]].pos != -1 ) {
            fprintf ( file, "%d", vertices[indices[i + 2]].pos + 1 );
        }

        fprintf ( file, "/" );

        if ( vertices[indices[i + 2]].tex != -1 ) {
            fprintf ( file, "%d", vertices[indices[i + 2]].tex + 1 );
        }

        fprintf ( file, "/" );

        if ( vertices[indices[i + 2]].norm != -1 ) {
            fprintf ( file, "%d", vertices[indices[i + 2]].norm + 1 );
        }

        fprintf ( file, "\n" );
    }

    fclose ( file );
}
#endif
#endif

#endif