#ifndef _APOLLO_H_
#define _APOLLO_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APOLLO_SUCCESS = 0,
    APOLLO_MATERIAL_FILE_NOT_FOUND = 1,
    APOLLO_FILE_NOT_FOUND = 2,
    APOLLO_MATERIAL_NOT_FOUND = 3,
    APOLLO_INVALID_FORMAT = 4,
    APOLLO_ERROR
} ApolloResult;

#define APOLLO_PATH 1024
#define APOLLO_NAME 256

#define APOLLO_TARGET_VERTEX_COUNT  (1 << 18)
#define APOLLO_TARGET_INDEX_COUNT   (1 << 18)
#define APOLLO_TARGET_MESH_COUNT    128

typedef struct {
    float x;
    float y;
} ApolloFloat2;

bool apollo_equalf2 ( const ApolloFloat2* a, const ApolloFloat2* b ) {
    return a->x == b->x && a->y == b->y;
}

ApolloFloat2 apollo_f2_set ( float x, float y ) {
    ApolloFloat2 f2;
    f2.x = x;
    f2.y = y;
    return f2;
}

typedef struct {
    float x;
    float y;
    float z;
} ApolloFloat3;

bool apollo_equalf3 ( const ApolloFloat3* a, const ApolloFloat3* b ) {
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

ApolloFloat3 apollo_f3_set ( float x, float y, float z ) {
    ApolloFloat3 f3;
    f3.x = x;
    f3.y = y;
    f3.z = z;
    return f3;
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

typedef size_t ApolloIndex;

typedef struct {
    ApolloFloat3 pos;
    ApolloFloat2 tex;
    ApolloFloat3 norm;
} ApolloVertex;

typedef struct {
    ApolloIndex a;
    ApolloIndex b;
    ApolloIndex c;
    ApolloFloat3 normal;
    ApolloFloat3 tangent;
    ApolloFloat3 bitangent;
} ApolloMeshFace;

typedef struct {
    ApolloMeshFace* faces;
    size_t face_count;
    size_t material_id;
    ApolloAABB aabb;
    ApolloSphere bounding_sphere;
} ApolloMesh;

typedef struct {
    ApolloVertex* vertices;
    size_t vertex_count;
    ApolloMesh* meshes;
    size_t mesh_count;
    ApolloAABB aabb;
    ApolloSphere bounding_sphere;
} ApolloModel;

// ------------------------------------------------------------------------------------------------------
// Materials
// A subset of the mtl obj specification http://paulbourke.net/dataformats/mtl
// It's a little bit weird as the PBR extension does not define an illumination model
// APOLLO_PBR is then returned whenever one of the PBR attributes (from Pr onwards) is encountered
// ------------------------------------------------------------------------------------------------------
typedef enum {
    APOLLO_DIFFUSE,
    APOLLO_SPECULAR,
    APOLLO_MIRROR,
    APOLLO_PBR
} ApolloBSDF;

// Path-tracer subset of http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr
typedef struct {
    float        ior;                   // Ni
    ApolloFloat3 diffuse;               // Kd
    int          diffuse_map_id;        // map_Kd
    ApolloFloat3 specular;              // Ks
    int          specular_map_id;       // map_Ks
    float        specular_exp;          // Ns [0 1000]
    int          specular_exp_map_id;   // map_Ns
    int          bump_map_id;           // bump
    int          disp_map_id;           // disp
    float        roughness;             // Pr
    int          roughness_map_id;      // map_Pr
    float        metallic;              // Pm
    int          metallic_map_id;       // map_Pm
    ApolloFloat3 emissive;              // Ke
    int          emissive_map_id;       // map_Ke
    int          normal_map_id;         // norm
    ApolloBSDF   bsdf;
    char         name[APOLLO_NAME];
} ApolloMaterial;

typedef struct {
    char name[APOLLO_NAME];
} ApolloTexture;

size_t  apollo_buffer_size ( void* buffer );
void    apollo_buffer_free ( void* buffer );

ApolloResult apollo_import_model_obj ( const char* filename, ApolloModel* model,
                                       ApolloMaterial** materials, ApolloTexture** textures,
                                       bool right_handed_coords, bool flip_faces );

#ifdef __cplusplus
}
#endif

#endif

//--------------------------------------------------------------------------------------------------
// _____                 _                           _        _   _
//|_   _|               | |                         | |      | | (_)
//  | |  _ __ ___  _ __ | | ___ _ __ ___   ___ _ __ | |_ __ _| |_ _  ___  _ __
//  | | | '_ ` _ \| '_ \| |/ _ \ '_ ` _ \ / _ \ '_ \| __/ _` | __| |/ _ \| '_ \ 
// _| |_| | | | | | |_) | |  __/ | | | | |  __/ | | | || (_| | |_| | (_) | | | |
//|_____|_| |_| |_| .__/|_|\___|_| |_| |_|\___|_| |_|\__\__,_|\__|_|\___/|_| |_|
//                | |
//                |_|
//--------------------------------------------------------------------------------------------------

#ifdef APOLLO_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
// Implementation-local header
//--------------------------------------------------------------------------------------------------

#ifndef APOLLO_LOG_ERR
#define APOLLO_LOG_ERR(fmt, ...) fprintf(stderr, "Apollo Error: " fmt "\n", __VA_ARGS__)
#endif

#ifndef APOLLO_LOG_WARN
#define APOLLO_LOG_WARN(fmt, ...) fprintf(stdout, "Apollo Warning: " fmt "\n", __VA_ARGS__)
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

size_t          apollo_find_texture ( const ApolloTexture* textures, const char* texture_name );
size_t          apollo_find_material ( const ApolloMaterial* materials, const char* mat_name );

ApolloResult    apollo_read_texture ( FILE* file, ApolloTexture* textures, size_t* idx_out );
bool            apollo_read_float2 ( FILE* file, ApolloFloat2* val );
bool            apollo_read_float3 ( FILE* file, ApolloFloat3* val );

// Material Libraries are files containing one or more material definitons.
// Model files like obj shall then reference those materials by name.
typedef struct {
    FILE* file;
    ApolloMaterial* materials;
    char filename[APOLLO_PATH];
} ApolloMaterialLib;

ApolloResult    apollo_open_material_lib ( const char* filename, ApolloMaterialLib* lib, ApolloTexture* textures );

// Tables are used to speed up model load times
typedef struct {
    size_t count;
    size_t capacity;
    size_t access_mask;
    bool is_zero_id_slot_used;
    ApolloIndex* items;
} ApolloIndexTable;

void            apollo_index_table_create ( ApolloIndexTable* table, size_t capacity );
void            apollo_index_table_destroy ( ApolloIndexTable* table );
void            apollo_index_table_clear ( ApolloIndexTable* table );
uint64_t        apollo_index_hash ( ApolloIndex key );
ApolloIndex*    apollo_index_table_get_item ( const ApolloIndexTable* table, uint32_t hash );
ApolloIndex*    apollo_index_table_get_next ( const ApolloIndexTable* table, ApolloIndex* item );
size_t          apollo_index_table_item_distance ( const ApolloIndexTable* table, ApolloIndex* a, ApolloIndex* b );
void            apollo_index_table_resize ( ApolloIndexTable* table, size_t new_capacity );
void            apollo_index_table_insert ( ApolloIndexTable* table, ApolloIndex item );
bool            apollo_index_table_lookup ( ApolloIndexTable* table, ApolloIndex item );

typedef struct {
    ApolloFloat3 key;
    uint32_t value;
} ApolloVertexTableItem;

typedef struct {
    size_t count;
    size_t capacity;
    size_t access_mask;
    bool is_zero_key_item_used;
    ApolloVertexTableItem zero_key_item;
    ApolloVertexTableItem* items;
} ApolloVertexTable;

void                    apollo_vertex_table_create ( ApolloVertexTable* table, size_t capacity );
void                    apollo_vertex_table_destroy ( ApolloVertexTable* table );
uint32_t                apollo_f3_hash ( const ApolloFloat3* v );
ApolloVertexTableItem*  apollo_vertex_table_get_item ( const ApolloVertexTable* table, uint32_t hash );
ApolloVertexTableItem*  apollo_vertex_table_get_next ( const ApolloVertexTable* table, ApolloVertexTableItem* item );
int                     apollo_vertex_table_item_distance ( const ApolloVertexTable* table, ApolloVertexTableItem* a, ApolloVertexTableItem* b );
void                    apollo_vertex_table_resize ( ApolloVertexTable* table, size_t new_capacity );
void                    apollo_vertex_table_insert ( ApolloVertexTable* table, const ApolloVertexTableItem* item );
// Returns a pointer to the element inside the table with matching key. The pointer can (and eventually will) be invalidated upon insertion.
ApolloVertexTableItem*  apollo_vertex_table_lookup ( ApolloVertexTable* table, const ApolloFloat3* key );

// AdjacencyTable
#define APOLLO_ADJACENCY_EXTENSION_CAPACITY 13
#define APOLLO_ADJACENCY_ITEM_CAPACITY 11

typedef struct ApolloAdjacencyTableExtension {
    uint32_t count;
    uint32_t values[APOLLO_ADJACENCY_EXTENSION_CAPACITY];   // face indices
    struct ApolloAdjacencyTableExtension* next;
} ApolloAdjacencyTableExtension;

typedef struct {
    ApolloIndex key;
    uint32_t count;
    uint32_t values[APOLLO_ADJACENCY_ITEM_CAPACITY];    // face indices
    ApolloAdjacencyTableExtension* next;
} ApolloAdjacencyTableItem;

// returns false if it was already there, false if not. After the search, if not found, the item is added.
bool        apollo_adjacency_item_add_unique ( ApolloAdjacencyTableItem* item, uint32_t value );
// returns true if it was already there, false if not. After the search, if not found, the item is added.
bool        apollo_adjacency_item_contains ( ApolloAdjacencyTableItem* item, uint32_t value );

typedef struct {
    size_t count;
    size_t capacity;
    size_t access_mask;
    bool is_zero_key_item_used;
    ApolloAdjacencyTableItem zero_key_item;
    ApolloAdjacencyTableItem* items;
} ApolloAdjacencyTable;

void                        apollo_adjacency_table_create ( ApolloAdjacencyTable* table, size_t capacity );
void                        apollo_adjacency_table_destroy ( ApolloAdjacencyTable* table );
ApolloAdjacencyTableItem*   apollo_adjacency_table_get_item ( ApolloAdjacencyTable* table, uint32_t hash );
ApolloAdjacencyTableItem*   apollo_adjacency_table_get_next ( ApolloAdjacencyTable* table, ApolloAdjacencyTableItem* item );
int                         apollo_adjacency_table_distance ( const ApolloAdjacencyTable* table, const ApolloAdjacencyTableItem* a, const ApolloAdjacencyTableItem* b );
void                        apollo_adjacency_table_resize ( ApolloAdjacencyTable* table, size_t new_capacity );
ApolloAdjacencyTableItem*   apollo_adjacency_table_insert ( ApolloAdjacencyTable* table, ApolloIndex key );
ApolloAdjacencyTableItem*   apollo_adjacency_table_lookup ( ApolloAdjacencyTable* table, ApolloIndex key );

// https://github.com/nothings/stb/blob/master/stretchy_buffer.h
#define sb_free     stb_sb_free
#define sb_push     stb_sb_push
#define sb_count    stb_sb_count
#define sb_add      stb_sb_add
#define sb_last     stb_sb_last
#define sb_prealloc stb_sb_prealloc

#define stb_sb_free(a)          ((a) ? free(stb__sbraw(a)),0 : 0)
#define stb_sb_push(a,v)        (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_count(a)         ((a) ? stb__sbn(a) : 0)
#define stb_sb_add(a,n)         (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define stb_sb_last(a)          ((a)[stb__sbn(a)-1])
#define stb_sb_prealloc(a,n)    (stb__sbgrow(a,n))

#define stb__sbraw(a)           ((int *) (a) - 2)
#define stb__sbm(a)             stb__sbraw(a)[0]
#define stb__sbn(a)             stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)    ((a)==0 || stb__sbn(a)+(n) >= stb__sbm(a))
#define stb__sbmaybegrow(a,n)   (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : 0)
#define stb__sbgrow(a,n)        (*((void **)&(a)) = stb__sbgrowf((a), (n), sizeof(*(a))))

void* stb__sbgrowf ( void* arr, int increment, int itemsize );

//--------------------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------------------
size_t apollo_find_texture ( const ApolloTexture* textures, const char* texture_name ) {
    for ( size_t i = 0; i < sb_count ( textures ); ++i ) {
        if ( strcmp ( texture_name, textures[i].name ) == 0 ) {
            return i;
        }
    }

    return SIZE_MAX;
}

size_t apollo_find_material ( const ApolloMaterial* materials, const char* mat_name ) {
    for ( size_t i = 0; i < sb_count ( materials ); ++i ) {
        if ( strcmp ( mat_name, materials[i].name ) == 0 ) {
            return i;
        }
    }

    return SIZE_MAX;
}

size_t apollo_find_material_lib ( const ApolloMaterialLib* libs, const char* lib_filename ) {
    for ( size_t i = 0; i < sb_count ( libs ); ++i ) {
        if ( strcmp ( lib_filename, libs[i].filename ) == 0 ) {
            return i;
        }
    }

    return SIZE_MAX;
}

size_t  apollo_buffer_size ( void* buffer ) {
    return sb_count ( buffer );
}

void apollo_buffer_free ( void* buffer ) {
    sb_free ( buffer );
}

//--------------------------------------------------------------------------------------------------
// File parsing routines
//--------------------------------------------------------------------------------------------------
// TODO: The overall error reporting should be improved, once finer grained
// APOLLO_ERRORs are spread around they should also be moved here.
// >= 0 valid texture index
// -1 -> format error
// -2 -> texture error
ApolloResult apollo_read_texture ( FILE* file, ApolloTexture* textures, size_t* idx_out ) {
    char map_name[APOLLO_NAME];

    if ( fscanf ( file, "%s", map_name ) != 1 ) {
        return APOLLO_INVALID_FORMAT;
    }

    size_t idx = apollo_find_texture ( textures, map_name );

    if ( idx == SIZE_MAX ) {
        idx = sb_count ( textures );
        ApolloTexture* texture = sb_add ( textures, 1 );
        strncpy ( texture->name, map_name, 256 );
    }

    *idx_out = idx;
    return APOLLO_SUCCESS;
}

bool apollo_read_float2 ( FILE* file, ApolloFloat2* val ) {
    float x, y, z;

    if ( fscanf ( file, "%f %f", &x, &y ) != 2 ) {
        return false;
    }

    val->x = x;
    val->y = y;
    return true;
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

//--------------------------------------------------------------------------------------------------
// Material
//--------------------------------------------------------------------------------------------------
void apollo_material_clear ( ApolloMaterial* mat ) {
    mat->bump_map_id = -1;
    mat->diffuse_map_id = -1;
    mat->disp_map_id = -1;
    mat->emissive_map_id = -1;
    mat->metallic_map_id = -1;
    mat->normal_map_id = -1;
    mat->roughness_map_id = -1;
    mat->specular_map_id = -1;
    mat->specular_exp_map_id = -1;
    mat->name[0] = '\0';
}

ApolloResult apollo_open_material_lib ( const char* filename, ApolloMaterialLib* lib, ApolloTexture* textures ) {
    FILE* file = NULL;
    file = fopen ( filename, "r" );

    if ( file == NULL ) {
        APOLLO_LOG_ERR ( "Cannot open file %s\n", filename );
        return APOLLO_ERROR;
    }

    char key[32];
    size_t key_len = 0;
    ApolloMaterial* materials = NULL;

    while ( fscanf ( file, "%s", key ) != EOF ) {
        // New material tag
        if ( strcmp ( key, "mat" ) == 0 || strcmp ( key, "newmtl" ) == 0 ) {
            char mat_name[32];

            if ( fscanf ( file, "%s", mat_name ) != 1 ) {
                APOLLO_LOG_ERR ( "Read error on file %s\n", filename );
                goto error;
            }

            for ( size_t i = 0; i < sb_count ( materials ); ++i ) {
                if ( strcmp ( mat_name, materials[i].name ) == 0 ) {
                    APOLLO_LOG_ERR ( "Duplicate material name (%s) in materials library %s\n", mat_name, filename );
                    goto error;
                }
            }

            size_t mat_name_len = strlen ( mat_name );
            ApolloMaterial* mat = sb_add ( materials, 1 );
            apollo_material_clear ( mat );
            strcpy ( mat->name, mat_name );
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

            sb_last ( materials ).diffuse = Kd;
        } else if ( strcmp ( key, "map_Kd" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).diffuse_map_id = idx;
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

            sb_last ( materials ).specular = Kd;
        } else if ( strcmp ( key, "map_Ks" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).specular_map_id = idx;
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

            sb_last ( materials ).specular_exp = Ns;
        }
        // Bump mapping
        else if ( strcmp ( key, "bump" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).bump_map_id = idx;
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

            sb_last ( materials ).roughness = Pr;
            sb_last ( materials ).bsdf = APOLLO_PBR;
        } else if ( strcmp ( key, "map_Pr" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).roughness_map_id = idx;
                sb_last ( materials ).bsdf = APOLLO_PBR;
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

            sb_last ( materials ).metallic = Pm;
            sb_last ( materials ).bsdf = APOLLO_PBR;
        } else if ( strcmp ( key, "map_Pm" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).metallic_map_id = idx;
                sb_last ( materials ).bsdf = APOLLO_PBR;
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

            sb_last ( materials ).emissive = Ke;
            sb_last ( materials ).bsdf = APOLLO_PBR;
        } else if ( strcmp ( key, "map_Ke" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).emissive_map_id = idx;
                sb_last ( materials ).bsdf = APOLLO_PBR;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading emissive texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Normal map
        else if ( strcmp ( key, "normal" ) == 0 ) {
            size_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).normal_map_id = idx;
                sb_last ( materials ).bsdf = APOLLO_PBR;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading normal texture on file %s\n", idx, filename );
                goto error;
            }
        } else if ( strcmp ( key, "d" ) == 0 || strcmp ( key, "Tr" ) == 0 || strcmp ( key, "Tf" ) == 0 || strcmp ( key, "Ka" ) == 0 ) {
            while ( getc ( file ) != '\n' );

            // ...
        } else if ( strcmp ( key, "illum" ) == 0 ) {
            if ( sb_last ( materials ).bsdf == APOLLO_PBR ) {
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
                    sb_last ( materials ).bsdf = APOLLO_SPECULAR;
                    break;

                case 5:
                    sb_last ( materials ).bsdf = APOLLO_MIRROR;
                    break;

                default:
                    sb_last ( materials ).bsdf = APOLLO_DIFFUSE;
                    break;
            }
        } else {
            APOLLO_LOG_ERR ( "Unexpected field name %s in materials file %s\n", key, filename );
            goto error;
        }
    }

    lib->file = file;
    strcpy ( lib->filename, filename );
    lib->materials = materials;
    return APOLLO_SUCCESS;
error:
    free ( materials );
    return APOLLO_ERROR;
}

//--------------------------------------------------------------------------------------------------
// Model
//--------------------------------------------------------------------------------------------------
ApolloResult apollo_import_model_obj ( const char* filename, ApolloModel* model, ApolloMaterial** _materials, ApolloTexture** _textures, bool rh, bool flip_faces ) {
    FILE* file = NULL;
    file = fopen ( filename, "r" );

    if ( file == NULL ) {
        fprintf ( stderr, "Cannot open file %s", filename );
        return APOLLO_FILE_NOT_FOUND;
    }

    size_t filename_len = strlen ( filename );
    const char* dir_end = filename + filename_len;

    while ( --dir_end != filename && *dir_end != '/' && *dir_end != '\\' )
        ;

    assert ( filename_len < APOLLO_PATH );
    char base_dir[APOLLO_PATH];
    base_dir[0] = '\0';
    size_t base_dir_len = 0;

    if ( dir_end != filename ) {
        ++dir_end;
        base_dir_len = dir_end - filename;
        strncpy ( base_dir, filename, base_dir_len );
    }

    // Temporary tables
    ApolloVertexTable vtable;
    ApolloAdjacencyTable atable;
    ApolloIndexTable itable;
    // Temporary buffers
    ApolloMaterial* materials = *_materials;
    ApolloTexture* textures = *_textures;
    ApolloFloat3* vertex_pos = NULL;
    ApolloFloat2* tex_coords = NULL;
    ApolloFloat3* normals = NULL;
    size_t face_count = 0;
    typedef struct {
        size_t smoothing_group;
        size_t material;
        size_t indices_offset;
    } Mesh;
    Mesh* meshes = NULL;
    ApolloMaterial* used_materials = NULL;  // New ApolloMaterials go from material_libs to here while building, and from here to materials before returning.
    ApolloMaterialLib* material_libs = NULL;
    // Non-temporary buffers (part of the ApolloModel)
    ApolloIndex* indices = NULL;
    ApolloVertex* vertices = NULL;
    // Prealloc memory
    apollo_vertex_table_create ( &vtable, 2 * APOLLO_TARGET_VERTEX_COUNT );
    apollo_adjacency_table_create ( &atable, 2 * APOLLO_TARGET_VERTEX_COUNT );
    apollo_index_table_create ( &itable, APOLLO_TARGET_INDEX_COUNT );
    sb_prealloc ( vertex_pos, APOLLO_TARGET_VERTEX_COUNT );
    sb_prealloc ( tex_coords, APOLLO_TARGET_VERTEX_COUNT );
    sb_prealloc ( normals, APOLLO_TARGET_VERTEX_COUNT );
    sb_prealloc ( indices, APOLLO_TARGET_INDEX_COUNT );
    sb_prealloc ( vertices, APOLLO_TARGET_VERTEX_COUNT );
    sb_prealloc ( meshes, APOLLO_TARGET_MESH_COUNT );
    sb_prealloc ( used_materials, APOLLO_TARGET_MESH_COUNT );
    sb_prealloc ( material_libs, APOLLO_TARGET_MESH_COUNT );
    ApolloResult result;
    int x;
    bool material_loaded = false;

    while ( ( x = fgetc ( file ) ) != EOF ) {
        switch ( x ) {
            case '#': // Comment. Skip the line.
                while ( fgetc ( file ) != '\n' )
                    ;

                break;

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

                char lib_name[APOLLO_PATH];
                memcpy ( lib_name, base_dir, base_dir_len );
                fscanf ( file, "%s", lib_name + base_dir_len );
                size_t lib_name_len = strlen ( lib_name );
                size_t idx = apollo_find_material_lib ( material_libs, lib_name );

                if ( idx == SIZE_MAX ) {
                    result = apollo_open_material_lib ( lib_name, sb_add ( material_libs, 1 ), textures );

                    if ( result != APOLLO_SUCCESS ) {
                        goto error;
                    }
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

                // Chech for mesh
                if ( sb_count ( meshes ) == 0 ) {
                    Mesh* m = sb_add ( meshes, 1 );
                    m->indices_offset = sb_count ( indices );
                    m->material = SIZE_MAX;
                    m->smoothing_group = SIZE_MAX;
                }

                // Lookup the user materials
                size_t idx = apollo_find_material ( materials, mat_name );

                if ( idx != SIZE_MAX ) {
                    sb_last ( meshes ).material = idx;
                    break;
                }

                // Lookup the additional materials that have been already loaded for this obj
                idx = apollo_find_material ( used_materials, mat_name );

                if ( idx != SIZE_MAX ) {
                    sb_last ( meshes ).material = idx + sb_count ( materials );
                    break;
                }

                // Lookup the entire material libs that have been linked so far by this obj
                for ( size_t i = 0; i < sb_count ( material_libs ); ++i ) {
                    idx = apollo_find_material ( material_libs[i].materials, mat_name );

                    if ( idx != SIZE_MAX ) {
                        sb_push ( used_materials, material_libs[i].materials[idx] );
                        sb_last ( meshes ).material = sb_count ( materials ) + sb_count ( used_materials ) - 1;
                        break;
                    }
                }

                if ( idx == SIZE_MAX ) {
                    result = APOLLO_MATERIAL_NOT_FOUND;
                    APOLLO_LOG_ERR ( "Failed to find material %s @ %s", mat_name, filename );
                    goto error;
                }
            }
            break;

            case 'v': {
                // Vertex data.
                x = fgetc ( file );

                switch ( x ) {
                    case ' ': {
                        ApolloFloat3* pos = sb_add ( vertex_pos, 1 );

                        if ( !apollo_read_float3 ( file, pos ) ) {
                            result = APOLLO_INVALID_FORMAT;
                            goto error;
                        }

                        if ( rh ) {
                            pos->z = pos->z * -1.f;
                        }
                    }
                    break;

                    case 't': {
                        ApolloFloat2* uv = sb_add ( tex_coords, 1 );

                        if ( !apollo_read_float2 ( file, uv ) ) {
                            result = APOLLO_INVALID_FORMAT;
                            goto error;
                        }

                        if ( rh ) {
                            uv->y = 1.f - uv->y;
                        }
                    }
                    break;

                    case 'n': {
                        ApolloFloat3* norm = sb_add ( normals, 1 );

                        if ( !apollo_read_float3 ( file, norm ) ) {
                            result = APOLLO_INVALID_FORMAT;
                            goto error;
                        }

                        if ( rh ) {
                            norm->z = norm->z * -1;
                        }
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
                        Mesh* m = sb_add ( meshes, 1 );
                        m->indices_offset = sb_count ( indices );
                        break;
                }

                while ( fgetc ( file ) != '\n' )
                    ;

                break;

            case 's':
                x = fgetc ( file );
                x = fgetc ( file );

                if ( sb_count ( meshes ) == 0 ) {
                    Mesh* m = sb_add ( meshes, 1 );
                    m->indices_offset = sb_count ( indices );
                    m->material = SIZE_MAX;
                    m->smoothing_group = SIZE_MAX;
                }

                if ( x == '1' ) {
                    sb_last ( meshes ).smoothing_group = 1;
                } else {
                    sb_last ( meshes ).smoothing_group = 0;
                }

                while ( fgetc ( file ) != '\n' )
                    ;

                break;

            case 'f':
                // Face definition. Basically a tuple of indices that make up a single face (triangle).
                // Example: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3.
                //  vi: index of i-th vertex position. vti: index of i-th vertex texcoord. vni: index of i-th vertex normal.
                //  The i is local to the face (usually faces are triangular so it goes from 1 to 3).
                x = fgetc ( file );

                if ( x != ' ' ) {
                    APOLLO_LOG_ERR ( "Expected space after 'f', but found %c", x );
                    result = APOLLO_INVALID_FORMAT;
                    goto error;
                }

                {
                    // We are about to process the entire face.
                    // We read the entire face line from the file and store it.
                    // We then prepare to go through the face string, storing and processing one vertex at a time.
                    char face_str[128];
                    fgets ( face_str, 128, file );
                    size_t face_len = strlen ( face_str );
                    size_t face_vertex_count = 0;   // To increase on read from face
                    size_t first_vertex_index = -1;
                    size_t last_vertex_index = -1;
                    size_t offset = 0;

                    // Foreach vertex in face
                    // We assume there are at least 3 vertices per face (seems fair)
                    while ( offset < face_len ) {
                        size_t pos_index = -1;
                        size_t tex_index = -1;
                        size_t norm_index = -1;
                        char vertex_str[64];
                        sscanf ( face_str + offset, "%s", vertex_str );
                        size_t vertex_len = strlen ( vertex_str );
                        offset += vertex_len + 1;

                        // If the face is not a triangle, we need to split it up into triangles.
                        // To do so, we always use the first and last vetices, plus the one we're currently processing.
                        // first_vertex_index and last_vertex_index will be set later (note the face_vertex_count >=3 check).
                        // TODO ?
                        if ( face_vertex_count >= 3 ) {
                            sb_push ( indices, first_vertex_index );
                            sb_push ( indices, last_vertex_index );
                            //indices[index_count++] = first_vertex_index;
                            //indices[index_count++] = last_vertex_index;
                        }

                        // A new vertex string has been extracted from the face string.
                        // We need to do one more xformation: divide the vertex string into the data pieces the vertex is composed of.
                        // We prepare to go through the vertex string, storing and processing one data piece at a time.
                        // Parse the vertex
                        char data_piece[10];
                        size_t data_piece_len = 0;     // To reset after each processed data piece
                        size_t data_piece_index = 0;   // To increase after each processed data piece
                        {
                            size_t i = 0;

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
                        // Preallocate index
                        ApolloIndex* index = sb_add ( indices, 1 );

                        // In an OBJ file, the faces mark the end of a mesh.
                        // However it could be that the first mesh was implicitly started (not listed in the file), so we consider that case,
                        if ( sb_count ( meshes ) == 0 ) {
                            Mesh* m = sb_add ( meshes, 1 );
                            m->indices_offset = sb_count ( indices ) - 1;
                            m->material = SIZE_MAX;
                            m->smoothing_group = SIZE_MAX;
                        }

                        // Finalize the vertex, testing for overlaps if the mesh is smooth and updating the first/last face vertex indices
                        bool duplicate = false;

                        if ( sb_last ( meshes ).smoothing_group == 1 ) {
                            ApolloVertexTableItem* lookup = apollo_vertex_table_lookup ( &vtable, &vertex_pos[pos_index] );

                            if ( lookup != NULL ) {
                                *index = lookup->value;
                                duplicate = true;
                            }
                        }

                        // If it's not a duplicate, add it as a new vertex
                        ApolloVertex* vertex = sb_add ( vertices, 1 );

                        if ( !duplicate ) {
                            vertex->pos = vertex_pos[pos_index];
                            vertex->tex = tex_index != -1 ? tex_coords[tex_index] : apollo_f2_set ( 0, 0 );
                            vertex->norm = norm_index != -1 ? normals[norm_index] : apollo_f3_set ( 0, 0, 0 );
                            *index = sb_count ( vertices ) - 1;
                            ApolloVertexTableItem item = { vertex->pos, sb_count ( vertices ) - 1 };
                            apollo_vertex_table_insert ( &vtable, &item );
                        }

                        // Store the index into the adjacency table
                        ApolloAdjacencyTableItem* adj_lookup = apollo_adjacency_table_lookup ( &atable, *index );

                        if ( adj_lookup == NULL ) {
                            adj_lookup = apollo_adjacency_table_insert ( &atable, *index );
                        }

                        apollo_adjacency_item_add_unique ( adj_lookup, face_count );

                        // Store the first and last vertex index for later
                        if ( first_vertex_index == -1 && face_vertex_count == 0 ) {
                            first_vertex_index = *index;
                        }

                        if ( face_vertex_count >= 2 ) {
                            last_vertex_index = *index;
                        }

                        ++face_vertex_count;

                        if ( face_vertex_count >= 3 ) {
                            ++face_count;
                        }
                    }
                }

                break;
        }
    }

    // End of file

    // Flip faces?
    if ( flip_faces ) {
        for ( size_t i = 0; i < sb_count ( indices ); i += 3 ) {
            ApolloIndex idx = indices[i];
            indices[i] = indices[i + 2];
            indices[i + 2] = idx;
        }
    }

    // The unnnecessary scoping is just so that it is possible to write goto error on error.
    {
        ApolloFloat3* face_normals = NULL;
        sb_add ( face_normals, face_count );
        ApolloFloat3* face_tangents = NULL;
        sb_add ( face_tangents, face_count );
        ApolloFloat3* face_bitangents = NULL;
        sb_add ( face_bitangents, face_count );

        // Compute face normals
        for ( size_t i = 0; i < face_count; ++i ) {
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
        for ( size_t m = 0; m < sb_count ( meshes ); ++m ) {
            size_t mesh_index_base = meshes[m].indices_offset;
            size_t mesh_index_count = m < sb_count ( meshes ) - 1 ? meshes[m + 1].indices_offset - mesh_index_base : sb_count ( indices ) - mesh_index_base;
            size_t mesh_face_count = mesh_index_count / 3;

            if ( mesh_index_count % 3 != 0 ) {
                APOLLO_LOG_ERR ( "Malformed .obj file @ %s", filename );
                result = APOLLO_INVALID_FORMAT;
                goto error;
            }

            // If the mesh isn't smooth, skip it
            if ( meshes[m].smoothing_group == 0 ) {
                for ( size_t i = 0; i < mesh_face_count; ++i ) {
                    vertices[indices[mesh_index_base + i * 3 + 0]].norm = face_normals[mesh_index_base / 3 + i];
                    vertices[indices[mesh_index_base + i * 3 + 1]].norm = face_normals[mesh_index_base / 3 + i];
                    vertices[indices[mesh_index_base + i * 3 + 2]].norm = face_normals[mesh_index_base / 3 + i];
                }

                continue;
            }

            // Vertex normals foreach mesh

            for ( size_t i = 0; i < mesh_index_count; ++i ) {
                ApolloFloat3 norm_sum;
                norm_sum.x = norm_sum.y = norm_sum.z = 0;
                // skip the index if the corresponding vertex has already been processed
                bool already_computed_vertex = apollo_index_table_lookup ( &itable, mesh_index_base + i );

                if ( already_computed_vertex ) {
                    continue;
                } else {
                    //computed_indices[computed_indices_count++] = indices[mesh_index_base + i];
                    apollo_index_table_insert ( &itable, mesh_index_base + i );
                }

                ApolloAdjacencyTableItem* adj_lookup = apollo_adjacency_table_lookup ( &atable, indices[mesh_index_base + i] );

                for ( size_t j = 0; j < adj_lookup->count; ++j ) {
                    size_t k = adj_lookup->values[j];
                    norm_sum.x += face_normals[k].x;
                    norm_sum.y += face_normals[k].y;
                    norm_sum.z += face_normals[k].z;
                }

                ApolloAdjacencyTableExtension* ext = adj_lookup->next;

                while ( ext != NULL ) {
                    for ( size_t j = 0; j < ext->count; ++j ) {
                        size_t k = ext->values[j];
                        norm_sum.x += face_normals[k].x;
                        norm_sum.y += face_normals[k].y;
                        norm_sum.z += face_normals[k].z;
                    }

                    ext = ext->next;
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

            //free ( computed_indices );
            apollo_index_table_clear ( &itable );
        }

        // Compute tangents and bitangents
        for ( size_t i = 0; i < face_count; ++i ) {
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

        // Update ApolloMaterials
        for ( size_t i = 0; i < sb_count ( used_materials ); ++i ) {
            sb_push ( materials, used_materials[i] );
        }

        // Build ApolloModel
        // Vertices
        model->vertices = ( ApolloVertex* ) malloc ( sizeof ( ApolloVertex ) * sb_count ( vertices ) );
        model->vertex_count = sb_count ( vertices );

        for ( size_t i = 0; i < sb_count ( vertices ); ++i ) {
            model->vertices[i].pos = vertices[i].pos;
            model->vertices[i].tex = vertices[i].tex;
            model->vertices[i].norm = vertices[i].norm;
        }

        // Meshes
        model->meshes = ( ApolloMesh* ) malloc ( sizeof ( ApolloMesh ) * sb_count ( meshes ) );
        model->mesh_count = sb_count ( meshes );

        for ( size_t i = 0; i < sb_count ( meshes ); ++i ) {
            // Faces
            int mesh_index_base = meshes[i].indices_offset;
            size_t mesh_index_count = i < sb_count ( meshes ) - 1 ? meshes[i + 1].indices_offset - mesh_index_base : sb_count ( indices ) - mesh_index_base;
            int mesh_face_count = mesh_index_count / 3;

            if ( mesh_index_count % 3 != 0 ) {
                APOLLO_LOG_ERR ( "Malformed mesh file @ %s", filename );
                result = APOLLO_INVALID_FORMAT;
                goto error;
            }

            model->meshes[i].faces = ( ApolloMeshFace* ) malloc ( sizeof ( ApolloMeshFace ) * mesh_face_count );
            model->meshes[i].face_count = mesh_face_count;

            for ( size_t j = 0; j < mesh_face_count; ++j ) {
                model->meshes[i].faces[j].a = indices[mesh_index_base + j * 3];
                model->meshes[i].faces[j].b = indices[mesh_index_base + j * 3 + 1];
                model->meshes[i].faces[j].c = indices[mesh_index_base + j * 3 + 2];
                model->meshes[i].faces[j].normal = face_normals[mesh_index_base / 3 + j];
                model->meshes[i].faces[j].tangent = face_tangents[mesh_index_base / 3 + j];
                model->meshes[i].faces[j].bitangent = face_bitangents[mesh_index_base / 3 + j];
            }

            // Material
            model->meshes[i].material_id = meshes[i].material;
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

            for ( size_t j = 1; j < mesh_index_count; ++j ) {
                ApolloFloat3 pos = vertices[indices[mesh_index_base + j]].pos;
                ApolloFloat3 center_to_pos;
                center_to_pos.x = pos.x - center.x;
                center_to_pos.y = pos.y - center.y;
                center_to_pos.z = pos.z - center.z;
                float dist = sqrtf ( center_to_pos.x * center_to_pos.x + center_to_pos.y * center_to_pos.y + center_to_pos.z * center_to_pos.z );

                if ( dist > max_dist ) {
                    max_dist = dist;
                }
            }

            model->meshes[i].bounding_sphere.center.x = center.x;
            model->meshes[i].bounding_sphere.center.y = center.y;
            model->meshes[i].bounding_sphere.center.z = center.z;
            model->meshes[i].bounding_sphere.radius = max_dist;
        }
    }
    // AABB
    {
        ApolloFloat3 min = vertices[0].pos;
        ApolloFloat3 max = vertices[0].pos;

        for ( size_t j = 1; j < sb_count ( vertices ); ++j ) {
            ApolloFloat3 pos = vertices[j].pos;
            min.x = pos.x < min.x ? pos.x : min.x;
            min.y = pos.y < min.y ? pos.y : min.y;
            min.z = pos.z < min.z ? pos.z : min.z;
            max.x = pos.x > max.x ? pos.x : max.x;
            max.y = pos.y > max.y ? pos.y : max.y;
            max.z = pos.z > max.z ? pos.z : max.z;
        }

        model->aabb.min = min;
        model->aabb.max = max;
    }
    // Sphere
    {
        ApolloFloat3 center;
        center.x = ( model->aabb.max.x + model->aabb.min.x ) / 2.f;
        center.y = ( model->aabb.max.y + model->aabb.min.y ) / 2.f;
        center.z = ( model->aabb.max.z + model->aabb.min.z ) / 2.f;
        float max_dist = 0;

        for ( size_t j = 1; j < sb_count ( vertices ); ++j ) {
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
    }
    fclose ( file );

    for ( size_t i = 0; i < sb_count ( material_libs ); ++i ) {
        fclose ( material_libs[i].file );
    }

    *_materials = materials;
    *_textures = textures;
    apollo_vertex_table_destroy ( &vtable );
    apollo_adjacency_table_destroy ( &atable );
    apollo_index_table_destroy ( &itable );
    sb_free ( used_materials );
    sb_free ( material_libs );
    sb_free ( vertex_pos );
    sb_free ( tex_coords );
    sb_free ( normals );
    sb_free ( indices );
    sb_free ( vertices );
    sb_free ( meshes );
    return APOLLO_SUCCESS;
error:
    apollo_vertex_table_destroy ( &vtable );
    apollo_adjacency_table_destroy ( &atable );
    apollo_index_table_destroy ( &itable );
    sb_free ( used_materials );
    sb_free ( material_libs );
    sb_free ( vertex_pos );
    sb_free ( tex_coords );
    sb_free ( normals );
    sb_free ( indices );
    sb_free ( vertices );
    sb_free ( meshes );
    return result;
}

//--------------------------------------------------------------------------------------------------
// IndexTable
//--------------------------------------------------------------------------------------------------
void apollo_index_table_create ( ApolloIndexTable* table, size_t capacity ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 ); // is power of 2
    table->items = ( ApolloIndex* ) malloc ( sizeof ( ApolloIndex ) * capacity );
    memset ( table->items, 0, sizeof ( ApolloIndex ) * capacity );
    table->capacity = capacity;
    table->count = 0;
    table->access_mask = capacity - 1;
    table->is_zero_id_slot_used = false;
}

void apollo_index_table_destroy ( ApolloIndexTable* table ) {
    free ( table->items );
    table->items = NULL;
}

void apollo_index_table_clear ( ApolloIndexTable* table ) {
    memset ( table->items, 0, sizeof ( ApolloIndex ) * table->capacity );
    table->is_zero_id_slot_used = false;
    table->count = 0;
}

uint64_t apollo_index_hash ( ApolloIndex key ) {
    // MurmerHash3
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
}

ApolloIndex* apollo_index_table_get_item ( const ApolloIndexTable* table, uint32_t hash ) {
    return table->items + ( hash & table->access_mask );
}

ApolloIndex* apollo_index_table_get_next ( const ApolloIndexTable* table, ApolloIndex* item ) {
    if ( item + 1 != table->items + table->capacity ) {
        return item + 1;
    } else {
        return table->items;
    }
}

size_t apollo_index_table_item_distance ( const ApolloIndexTable* table, ApolloIndex* a, ApolloIndex* b ) {
    if ( b >= a ) {
        return b - a;
    } else {
        return table->capacity + b - a;
    }
}

void apollo_index_table_resize ( ApolloIndexTable* table, size_t new_capacity ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 ); // is power of 2
    ApolloIndex* old_items = table->items;
    size_t old_capacity = table->capacity;
    table->items = ( ApolloIndex* ) malloc ( sizeof ( ApolloIndex ) * new_capacity );
    memset ( table->items, 0, sizeof ( ApolloIndex ) * new_capacity );
    table->capacity = new_capacity;
    table->access_mask = new_capacity - 1;

    for ( ApolloIndex* old_item = old_items; old_item != old_items + old_capacity; ++old_item ) {
        if ( *old_item != 0 ) { // is used
            uint32_t hash = apollo_index_hash ( *old_item );
            ApolloIndex* new_item = apollo_index_table_get_item ( table, hash );

            while ( *new_item != 0 ) { // find an unused slot
                new_item = apollo_index_table_get_next ( table, new_item );
            }

            *new_item = *old_item;
        }
    }

    free ( old_items );
}

void apollo_index_table_insert ( ApolloIndexTable* table, ApolloIndex item ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_index_table_resize ( table, table->capacity * 2 );
    }

    if ( item != 0 ) {
        uint32_t hash = apollo_index_hash ( item );
        ApolloIndex* dest = apollo_index_table_get_item ( table, hash );

        while ( *dest != 0 ) { // find an unused slot
            dest = apollo_index_table_get_next ( table, dest );
        }

        ++table->count;
        *dest = item;
    } else {
        table->is_zero_id_slot_used = true;
        ++table->count;
    }
}

bool apollo_index_table_lookup ( ApolloIndexTable* table, ApolloIndex item ) {
    if ( item != 0 ) {
        uint32_t hash = apollo_index_hash ( item );
        ApolloIndex* slot = apollo_index_table_get_item ( table, hash );

        while ( *slot != 0 && *slot != item ) { // find an unused slot or the item we're looking for
            slot = apollo_index_table_get_next ( table, slot );
        }

        if ( *slot == item ) {
            return true;
        }

        if ( *slot == 0 ) {
            return false;
        }
    } else {
        return table->is_zero_id_slot_used;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// VertexTable
//--------------------------------------------------------------------------------------------------
void apollo_vertex_table_create ( ApolloVertexTable* table, size_t capacity ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 ); // is power of 2
    table->items = ( ApolloVertexTableItem* ) malloc ( sizeof ( ApolloVertexTableItem ) * capacity );
    memset ( table->items, 0, sizeof ( *table->items ) * capacity );
    table->capacity = capacity;
    table->count = 0;
    table->access_mask = capacity - 1;
    table->is_zero_key_item_used = false;
    table->zero_key_item = { { 0, 0, 0 }, 0 };
}

void apollo_vertex_table_destroy ( ApolloVertexTable* table ) {
    free ( table->items );
    table->items = NULL;
}

uint32_t apollo_f3_hash ( const ApolloFloat3* v ) {
    const uint32_t* h = ( const uint32_t* ) v;
    uint32_t f = ( h[0] + h[1] * 11 - ( h[2] * 17 ) ) & 0x7fffffff; // avoid problems with +-0
    return ( f >> 22 ) ^ ( f >> 12 ) ^ ( f );
}

ApolloVertexTableItem* apollo_vertex_table_get_item ( const ApolloVertexTable* table, uint32_t hash ) {
    return table->items + ( hash & table->access_mask );
}

ApolloVertexTableItem* apollo_vertex_table_get_next ( const ApolloVertexTable* table, ApolloVertexTableItem* item ) {
    if ( item + 1 != table->items + table->capacity ) {
        return item + 1;
    } else {
        return table->items;
    }
}

int apollo_vertex_table_item_distance ( const ApolloVertexTable* table, ApolloVertexTableItem* a, ApolloVertexTableItem* b ) {
    if ( b >= a ) {
        return b - a;
    } else {
        return table->capacity + b - a;
    }
}

void apollo_vertex_table_resize ( ApolloVertexTable* table, size_t new_capacity ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 ); // is power of 2
    ApolloVertexTableItem* old_items = table->items;
    int old_capacity = table->capacity;
    table->items = ( ApolloVertexTableItem* ) malloc ( sizeof ( ApolloVertexTableItem ) * new_capacity );
    memset ( table->items, 0, sizeof ( *table->items ) * new_capacity );
    table->capacity = new_capacity;
    table->access_mask = new_capacity - 1;
    ApolloFloat3 zero_f3 = { 0, 0, 0 };

    for ( ApolloVertexTableItem* old_item = old_items; old_item != old_items + old_capacity; ++old_item ) {
        if ( !apollo_equalf3 ( &old_item->key, &zero_f3 ) ) { // is used
            uint32_t hash = apollo_f3_hash ( &old_item->key );
            ApolloVertexTableItem* new_item = apollo_vertex_table_get_item ( table, hash );

            while ( !apollo_equalf3 ( &new_item->key, &zero_f3 ) ) { // find an unused slot
                new_item = apollo_vertex_table_get_next ( table, new_item );
            }

            *new_item = *old_item;
        }
    }

    free ( old_items );
}

void apollo_vertex_table_insert ( ApolloVertexTable* table, const ApolloVertexTableItem* item ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_vertex_table_resize ( table, table->capacity * 2 );
    }

    ApolloFloat3 zero_f3 = { 0, 0, 0 };

    if ( !apollo_equalf3 ( &item->key, &zero_f3 ) ) {
        uint32_t hash = apollo_f3_hash ( &item->key );
        ApolloVertexTableItem* dest = apollo_vertex_table_get_item ( table, hash );

        while ( !apollo_equalf3 ( &dest->key, &zero_f3 ) ) { // find an unused slot
            dest = apollo_vertex_table_get_next ( table, dest );
        }

        ++table->count;
        *dest = *item;
    } else {
        table->is_zero_key_item_used = true;
        table->zero_key_item = *item;
        ++table->count;
    }
}

// Returns a pointer to the element inside the table with matching key. The pointer can (and eventually will) be invalidated upon insertion.
ApolloVertexTableItem* apollo_vertex_table_lookup ( ApolloVertexTable* table, const ApolloFloat3* key ) {
    ApolloFloat3 zero_f3 = { 0, 0, 0 };

    if ( !apollo_equalf3 ( key, &zero_f3 ) ) {
        uint32_t hash = apollo_f3_hash ( key );
        ApolloVertexTableItem* slot = apollo_vertex_table_get_item ( table, hash );

        while ( !apollo_equalf3 ( &slot->key, &zero_f3 ) && !apollo_equalf3 ( &slot->key, key ) ) { // find an unused slot or the item we're looking for
            slot = apollo_vertex_table_get_next ( table, slot );
        }

        if ( apollo_equalf3 ( &slot->key, key ) ) {
            return slot;
        }

        if ( apollo_equalf3 ( &slot->key, &zero_f3 ) ) {
            return NULL;
        }
    } else {
        if ( table->is_zero_key_item_used ) {
            return &table->zero_key_item;
        } else {
            return NULL;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
// AdjacencyTable
//--------------------------------------------------------------------------------------------------
// returns false if it was already there, false if not. After the search, if not found, the item is added.
bool apollo_adjacency_item_add_unique ( ApolloAdjacencyTableItem* item, uint32_t value ) {
    unsigned int i;

    for ( i = 0; i < item->count; ++i ) {
        if ( item->values[i] == value ) {
            return true;
        }
    }

    ApolloAdjacencyTableExtension* ext = item->next;

    while ( ext != NULL ) {
        for ( i = 0; i < ext->count; ++i ) {
            if ( ext->values[i] == value ) {
                return true;
            }
        }

        if ( ext->next == NULL ) {
            break;
        }

        ext = ext->next;
    }

    if ( ext == NULL ) {
        if ( item->count < APOLLO_ADJACENCY_ITEM_CAPACITY ) {
            item->values[item->count++] = value;
            return false;
        } else {
            item->next = ( ApolloAdjacencyTableExtension* ) malloc ( sizeof ( ApolloAdjacencyTableExtension ) );
            memset ( item->next, 0, sizeof ( *item->next ) );
            ext = item->next;
        }
    }

    if ( ext->count == APOLLO_ADJACENCY_EXTENSION_CAPACITY ) {
        ext->next = ( ApolloAdjacencyTableExtension* ) malloc ( sizeof ( ApolloAdjacencyTableExtension ) );
        memset ( ext->next, 0, sizeof ( *ext->next ) );
        ext = ext->next;
    }

    ext->values[ext->count++] = value;
    return false;
}

// returns true if it was already there, false if not. After the search, if not found, the item is added.
bool apollo_adjacency_item_contains ( ApolloAdjacencyTableItem* item, uint32_t value ) {
    unsigned int i;

    for ( i = 0; i < item->count; ++i ) {
        if ( item->values[i] == value ) {
            return true;
        }
    }

    ApolloAdjacencyTableExtension* ext = item->next;

    while ( ext != NULL ) {
        for ( i = 0; i < ext->count; ++i ) {
            if ( ext->values[i] == value ) {
                return true;
            }
        }

        ext = ext->next;
    }

    return false;
}

void apollo_adjacency_table_create ( ApolloAdjacencyTable* table, size_t capacity ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 );
    table->count = 0;
    table->capacity = capacity;
    table->access_mask = capacity - 1;
    table->items = ( ApolloAdjacencyTableItem* ) malloc ( sizeof ( ApolloAdjacencyTableItem ) * capacity );
    memset ( table->items, 0, sizeof ( ApolloAdjacencyTableItem ) * capacity );
    memset ( &table->zero_key_item, 0, sizeof ( table->zero_key_item ) );
}

void apollo_adjacency_table_destroy ( ApolloAdjacencyTable* table ) {
    free ( table->items );
    table->items = NULL;
}

ApolloAdjacencyTableItem* apollo_adjacency_table_get_item ( ApolloAdjacencyTable* table, uint32_t hash ) {
    return table->items + ( hash & table->access_mask );
}

ApolloAdjacencyTableItem* apollo_adjacency_table_get_next ( ApolloAdjacencyTable* table, ApolloAdjacencyTableItem* item ) {
    if ( item + 1 != table->items + table->capacity ) {
        return item + 1;
    } else {
        return table->items;
    }
}

int apollo_adjacency_table_distance ( const ApolloAdjacencyTable* table, const ApolloAdjacencyTableItem* a, const ApolloAdjacencyTableItem* b ) {
    if ( b >= a ) {
        return b - a;
    } else {
        return table->capacity + b - a;
    }
}

void apollo_adjacency_table_resize ( ApolloAdjacencyTable* table, size_t new_capacity ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 );
    ApolloAdjacencyTableItem* old_items = table->items;
    int old_capacity = table->capacity;
    table->items = ( ApolloAdjacencyTableItem* ) malloc ( sizeof ( ApolloAdjacencyTableItem ) * new_capacity );
    memset ( table->items, 0, sizeof ( ApolloAdjacencyTableItem ) * new_capacity );
    table->capacity = new_capacity;

    for ( ApolloAdjacencyTableItem* old_item = old_items; old_item != old_items + old_capacity; ++old_item ) {
        uint32_t hash = apollo_index_hash ( old_item->key );
        ApolloAdjacencyTableItem* new_item = apollo_adjacency_table_get_item ( table, hash );

        while ( new_item->key != 0 ) {
            new_item = apollo_adjacency_table_get_next ( table, new_item );
        }

        *new_item = *old_item;
    }

    free ( old_items );
}

ApolloAdjacencyTableItem* apollo_adjacency_table_insert ( ApolloAdjacencyTable* table, ApolloIndex key ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_adjacency_table_resize ( table, table->capacity * 2 );
    }

    if ( key != 0 ) {
        uint32_t hash = apollo_index_hash ( key );
        ApolloAdjacencyTableItem* item = apollo_adjacency_table_get_item ( table, hash );

        while ( item->key != 0 ) {
            item = apollo_adjacency_table_get_next ( table, item );
        }

        table->count++;
        item->key = key;
        return item;
    } else {
        table->is_zero_key_item_used = true;
        return &table->zero_key_item;
    }

    return NULL;
}

ApolloAdjacencyTableItem* apollo_adjacency_table_lookup ( ApolloAdjacencyTable* table, ApolloIndex key ) {
    if ( key != 0 ) {
        uint32_t hash = apollo_index_hash ( key );
        ApolloAdjacencyTableItem* item = apollo_adjacency_table_get_item ( table, hash );

        while ( item->key != 0 && item->key != key ) {
            item = apollo_adjacency_table_get_next ( table, item );
        }

        if ( item->key == key ) {
            return item;
        }

        if ( item->key == 0 ) {
            return NULL;
        }
    } else {
        if ( table->is_zero_key_item_used ) {
            return &table->zero_key_item;
        } else {
            return NULL;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
// STB SB
//--------------------------------------------------------------------------------------------------
static void* stb__sbgrowf ( void* arr, int increment, int itemsize ) {
    int dbl_cur = arr ? 2 * stb__sbm ( arr ) : 0;
    int min_needed = stb_sb_count ( arr ) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int* p = ( int* ) realloc ( arr ? stb__sbraw ( arr ) : 0, itemsize * m + sizeof ( int ) * 2 );

    if ( p ) {
        if ( !arr ) {
            p[1] = 0;
        }

        p[0] = m;
        return p + 2;
    } else {
        return ( void* ) ( 2 * sizeof ( int ) ); // try to force a NULL pointer exception later
    }
}

#ifdef __cplusplus
}
#endif

#endif