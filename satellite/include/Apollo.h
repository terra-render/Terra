#ifndef _APOLLO_H_
#define _APOLLO_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

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

#define APOLLO_PATH_LEN 1024
#define APOLLO_NAME_LEN 256

#define APOLLO_PREALLOC_VERTEX_COUNT  (1 << 18)
#define APOLLO_PREALLOC_INDEX_COUNT   (1 << 18)
#define APOLLO_PREALLOC_MESH_COUNT    128

typedef struct {
    uint32_t*   idx_a;
    uint32_t*   idx_b;
    uint32_t*   idx_c;
    float*      normal_x;
    float*      normal_y;
    float*      normal_z;
    float*      tangent_x;
    float*      tangent_y;
    float*      tangent_z;
    float*      bitangent_x;
    float*      bitangent_y;
    float*      bitangent_z;
} ApolloMeshFaceData;

typedef struct {
    ApolloMeshFaceData face_data;
    uint32_t    face_count;
    uint32_t    material_id;
    float       aabb_min[3];
    float       aabb_max[3];
    float       bounding_sphere[4];   // <x y z r>
    char        name[APOLLO_NAME_LEN];
} ApolloMesh;

typedef struct {
    float* pos_x;
    float* pos_y;
    float* pos_z;
    float* norm_x;
    float* norm_y;
    float* norm_z;
    float* tex_u;
    float* tex_v;
} ApolloModelVertexData;

typedef struct {
    ApolloModelVertexData vertex_data;
    uint32_t    vertex_count;
    uint32_t    mesh_count;
    float       aabb_min[3];
    float       aabb_max[3];
    float       bounding_sphere[4];   // <x y z r>
    ApolloMesh* meshes;
    char        name[APOLLO_NAME_LEN];
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
    float       ior;                    // Ni
    float       diffuse[3];             // Kd
    uint32_t    diffuse_texture;        // map_Kd
    float       specular[3];            // Ks
    uint32_t    specular_texture;       // map_Ks
    float       specular_exp;           // Ns [0 1000]
    uint32_t    specular_exp_texture;   // map_Ns
    uint32_t    bump_texture;           // bump
    uint32_t    displacement_texture;   // disp
    float       roughness;              // Pr
    uint32_t    roughness_texture;      // map_Pr
    float       metallic;               // Pm
    uint32_t    metallic_texture;       // map_Pm
    float       emissive[3];            // Ke
    uint32_t    emissive_texture;       // map_Ke
    uint32_t    normal_texture;         // norm
    ApolloBSDF  bsdf;
    char        name[APOLLO_NAME_LEN];
} ApolloMaterial;

#define APOLLO_TEXTURE_NONE UINT32_MAX

typedef struct {
    char name[APOLLO_NAME_LEN];
} ApolloTexture;

typedef void* ( apollo_alloc_fun ) ( void* allocator, size_t size, size_t alignment );
typedef void* ( apollo_realloc_fun ) ( void* allocator, void* addr, size_t old_size, size_t new_size, size_t new_alignment );
typedef void ( apollo_free_fun ) ( void* allocator, void* addr, size_t size );

size_t  apollo_buffer_size ( void* buffer );
void    apollo_buffer_free ( void* buffer );

typedef struct {
    void* temp_allocator;
    apollo_alloc_fun* temp_alloc;
    apollo_realloc_fun* temp_realloc;
    apollo_free_fun* temp_free;
    void* final_allocator;
    apollo_alloc_fun* final_alloc;
    bool flip_z;
    bool flip_texcoord_v;
    bool flip_faces;
} ApolloLoadOptions;

// The provided allocator is not required to strictly follow the alignment requirements. A malloc-based allocator is fine.
// If it does, the output data arrays are guaranteed to be 16-byte aligned.
ApolloResult apollo_import_model_obj ( const char* filename, ApolloModel* model, ApolloMaterial** materials, ApolloTexture** textures, const ApolloLoadOptions* options );

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

typedef struct {
    float x;
    float y;
} ApolloFloat2;

typedef struct {
    float x;
    float y;
    float z;
} ApolloFloat3;

inline bool apollo_equalf2 ( const ApolloFloat2* a, const ApolloFloat2* b ) {
    return a->x == b->x && a->y == b->y;
}

inline bool apollo_equalf3 ( const ApolloFloat3* a, const ApolloFloat3* b ) {
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

uint32_t        apollo_find_texture ( const ApolloTexture* textures, const char* texture_name );
uint32_t        apollo_find_material ( const ApolloMaterial* materials, const char* mat_name );

ApolloResult    apollo_read_texture ( FILE* file, ApolloTexture* textures, uint32_t* idx_out );
bool            apollo_read_float2 ( FILE* file, ApolloFloat2* val );
bool            apollo_read_float3 ( FILE* file, ApolloFloat3* val );

// Material Libraries are files containing one or more material definitons.
// Model files like obj shall then reference those materials by name.
typedef struct {
    FILE* file;
    ApolloMaterial* materials;
    char filename[APOLLO_PATH_LEN];
} ApolloMaterialLib;

ApolloResult    apollo_open_material_lib ( const char* filename, ApolloMaterialLib* lib, ApolloTexture* textures );
uint32_t        apollo_find_material_lib ( const ApolloMaterialLib* libs, const char* lib_filename );

// Tables are used to speed up model load times
// ApolloIndexTable is an index set, used to
typedef struct {
    uint32_t count;
    uint32_t capacity;
    uint32_t access_mask;
    bool is_zero_id_slot_used;
    uint32_t* items;
} ApolloIndexTable;

void            apollo_index_table_create ( ApolloIndexTable* table, uint32_t capacity );
void            apollo_index_table_destroy ( ApolloIndexTable* table );
void            apollo_index_table_clear ( ApolloIndexTable* table );
uint32_t        apollo_hash_u32 ( uint32_t key );
uint64_t        apollo_hash_u64 ( uint64_t key );
uint32_t*       apollo_index_table_get_item ( const ApolloIndexTable* table, uint32_t hash );
uint32_t*       apollo_index_table_get_next ( const ApolloIndexTable* table, uint32_t* item );
int             apollo_index_table_item_distance ( const ApolloIndexTable* table, uint32_t* a, uint32_t* b );
void            apollo_index_table_resize ( ApolloIndexTable* table, uint32_t new_capacity );
void            apollo_index_table_insert ( ApolloIndexTable* table, uint32_t item );
bool            apollo_index_table_lookup ( ApolloIndexTable* table, uint32_t item );

typedef struct {
    ApolloFloat3 key;
    uint32_t value;
} ApolloVertexTableItem;

typedef struct {
    uint32_t count;
    uint32_t capacity;
    uint32_t access_mask;
    bool is_zero_key_item_used;
    ApolloVertexTableItem zero_key_item;
    ApolloVertexTableItem* items;
} ApolloVertexTable;

void                    apollo_vertex_table_create ( ApolloVertexTable* table, uint32_t capacity );
void                    apollo_vertex_table_destroy ( ApolloVertexTable* table );
uint32_t                apollo_f3_hash ( const ApolloFloat3* v );
ApolloVertexTableItem*  apollo_vertex_table_get_item ( const ApolloVertexTable* table, uint32_t hash );
ApolloVertexTableItem*  apollo_vertex_table_get_next ( const ApolloVertexTable* table, ApolloVertexTableItem* item );
int                     apollo_vertex_table_item_distance ( const ApolloVertexTable* table, ApolloVertexTableItem* a, ApolloVertexTableItem* b );
void                    apollo_vertex_table_resize ( ApolloVertexTable* table, uint32_t new_capacity );
void                    apollo_vertex_table_insert ( ApolloVertexTable* table, const ApolloVertexTableItem* item );
// Returns a pointer to the element inside the table with matching key. The pointer can (and eventually will) be invalidated upon insertion.
ApolloVertexTableItem*  apollo_vertex_table_lookup ( ApolloVertexTable* table, const ApolloFloat3* key );

// AdjacencyTable
#define APOLLO_ADJACENCY_EXTENSION_CAPACITY 13
#define APOLLO_ADJACENCY_ITEM_CAPACITY 12

typedef struct ApolloAdjacencyTableExtension {
    uint32_t count;
    uint32_t values[APOLLO_ADJACENCY_EXTENSION_CAPACITY];   // face indices
    struct ApolloAdjacencyTableExtension* next;
} ApolloAdjacencyTableExtension;

typedef struct {
    uint32_t key;
    uint32_t count;
    uint32_t values[APOLLO_ADJACENCY_ITEM_CAPACITY];        // face indices
    ApolloAdjacencyTableExtension* next;
} ApolloAdjacencyTableItem;

// returns false if it was already there, false if not. After the search, if not found, the item is added.
bool        apollo_adjacency_item_add_unique ( ApolloAdjacencyTableItem* item, uint32_t value );
// returns true if it was already there, false if not. After the search, if not found, the item is added.
bool        apollo_adjacency_item_contains ( ApolloAdjacencyTableItem* item, uint32_t value );

typedef struct {
    uint32_t count;
    uint32_t capacity;
    uint32_t access_mask;
    bool is_zero_key_item_used;
    ApolloAdjacencyTableItem zero_key_item;
    ApolloAdjacencyTableItem* items;
} ApolloAdjacencyTable;

void                        apollo_adjacency_table_create ( ApolloAdjacencyTable* table, uint32_t capacity );
void                        apollo_adjacency_table_destroy ( ApolloAdjacencyTable* table );
ApolloAdjacencyTableItem*   apollo_adjacency_table_get_item ( ApolloAdjacencyTable* table, uint32_t hash );
ApolloAdjacencyTableItem*   apollo_adjacency_table_get_next ( ApolloAdjacencyTable* table, ApolloAdjacencyTableItem* item );
int                         apollo_adjacency_table_distance ( const ApolloAdjacencyTable* table, const ApolloAdjacencyTableItem* a, const ApolloAdjacencyTableItem* b );
void                        apollo_adjacency_table_resize ( ApolloAdjacencyTable* table, uint32_t new_capacity );
ApolloAdjacencyTableItem*   apollo_adjacency_table_insert ( ApolloAdjacencyTable* table, uint32_t key );
ApolloAdjacencyTableItem*   apollo_adjacency_table_lookup ( ApolloAdjacencyTable* table, uint32_t key );

void* apollo_temp_allocator = NULL;
apollo_alloc_fun* apollo_temp_alloc_f = NULL;
apollo_realloc_fun* apollo_temp_realloc_f = NULL;
apollo_free_fun* apollo_temp_free_f = NULL;

#define APOLLO_TEMP_ALLOC(T, n) (T*)apollo_temp_alloc_f(apollo_temp_allocator, sizeof(T) * n, alignof(T))
#define APOLLO_TEMP_ALLOC_ALIGN(T, n, a) (T*)apollo_temp_alloc_f(apollo_temp_allocator, sizeof(T) * n, a)
#define APOLLO_TEMP_FREE(p, n) apollo_temp_free_f(apollo_temp_allocator, p, sizeof(*p) * n);

// Custom version of https://github.com/nothings/stb/blob/master/stretchy_buffer.h
// Depends on apollo temp allocators
#define sb_free     stb_sb_free
#define sb_push     stb_sb_push
#define sb_count    stb_sb_count
#define sb_add      stb_sb_add
#define sb_last     stb_sb_last
#define sb_prealloc stb_sb_prealloc

#define stb_sb_free(a)          ((a) ? apollo_temp_free_f(apollo_temp_allocator,stb__sbraw(a),stb__sbm(a)),0 : 0)
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
uint32_t apollo_find_texture ( const ApolloTexture* textures, const char* texture_name ) {
    for ( size_t i = 0; i < sb_count ( textures ); ++i ) {
        if ( strcmp ( texture_name, textures[i].name ) == 0 ) {
            return i;
        }
    }

    return -1;
}

uint32_t apollo_find_material ( const ApolloMaterial* materials, const char* mat_name ) {
    for ( size_t i = 0; i < sb_count ( materials ); ++i ) {
        if ( strcmp ( mat_name, materials[i].name ) == 0 ) {
            return i;
        }
    }

    return -1;
}

uint32_t apollo_find_material_lib ( const ApolloMaterialLib* libs, const char* lib_filename ) {
    for ( size_t i = 0; i < sb_count ( libs ); ++i ) {
        if ( strcmp ( lib_filename, libs[i].filename ) == 0 ) {
            return i;
        }
    }

    return -1;
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

ApolloResult apollo_read_texture ( FILE* file, ApolloTexture* textures, uint32_t* idx_out ) {
    char name[APOLLO_NAME_LEN];

    if ( fscanf ( file, "%s", name ) != 1 ) {
        return APOLLO_INVALID_FORMAT;
    }

    uint32_t idx = apollo_find_texture ( textures, name );

    if ( idx == -1 ) {
        idx = sb_count ( textures );
        ApolloTexture* texture = sb_add ( textures, 1 );
        strncpy ( texture->name, name, 256 );
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
    mat->bump_texture = APOLLO_TEXTURE_NONE;
    mat->diffuse_texture = APOLLO_TEXTURE_NONE;
    mat->displacement_texture = APOLLO_TEXTURE_NONE;
    mat->emissive_texture = APOLLO_TEXTURE_NONE;
    mat->metallic_texture = APOLLO_TEXTURE_NONE;
    mat->normal_texture = APOLLO_TEXTURE_NONE;
    mat->roughness_texture = APOLLO_TEXTURE_NONE;
    mat->specular_texture = APOLLO_TEXTURE_NONE;
    mat->specular_exp_texture = APOLLO_TEXTURE_NONE;
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

            sb_last ( materials ).diffuse[0] = Kd.x;
            sb_last ( materials ).diffuse[1] = Kd.y;
            sb_last ( materials ).diffuse[2] = Kd.z;
        } else if ( strcmp ( key, "map_Kd" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).diffuse_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading diffuse texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Specular
        else if ( strcmp ( key, "Ks" ) == 0 ) {
            ApolloFloat3 Ks;

            if ( !apollo_read_float3 ( file, &Ks ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ks on file %s\n", filename );
                goto error;
            }

            sb_last ( materials ).specular[0] = Ks.x;
            sb_last ( materials ).specular[1] = Ks.y;
            sb_last ( materials ).specular[2] = Ks.z;
        } else if ( strcmp ( key, "map_Ks" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).specular_texture = idx;
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
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).bump_texture = idx;
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
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).roughness_texture = idx;
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
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).metallic_texture = idx;
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

            sb_last ( materials ).emissive[0] = Ke.x;
            sb_last ( materials ).emissive[1] = Ke.y;
            sb_last ( materials ).emissive[2] = Ke.z;
        } else if ( strcmp ( key, "map_Ke" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).emissive_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading emissive texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Normal map
        else if ( strcmp ( key, "normal" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, textures, &idx );

            if ( result == APOLLO_SUCCESS ) {
                sb_last ( materials ).normal_texture = idx;
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
                APOLLO_LOG_WARN ( "Skipping illum field; PBR BSDF has already been assumed." );

                while ( getc ( file ) != '\n' );

                continue;
            }

            int illum;

            if ( fscanf ( file, "%d", &illum ) != 1 ) {
                APOLLO_LOG_ERR ( "Failed to read illum on file %s", filename );
                goto error;
            }

            // Apollo pbr is handled at the end
            switch ( illum ) {
                case 1:
                    sb_last ( materials ).bsdf = APOLLO_DIFFUSE;
                    break;

                case 2:
                    sb_last ( materials ).bsdf = APOLLO_DIFFUSE;
                    break;

                case 5:
                    sb_last ( materials ).bsdf = APOLLO_MIRROR;
                    break;

                default:
                    sb_last ( materials ).bsdf = APOLLO_DIFFUSE;
                    break;
            }
        } else if ( strcmp ( key, "#" ) == 0 ) {
            while ( getc ( file ) != '\n' );
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
ApolloResult apollo_import_model_obj ( const char* filename, ApolloModel* model, ApolloMaterial** _materials, ApolloTexture** _textures, const ApolloLoadOptions* options ) {
    // Setup sb
    apollo_temp_allocator = options->temp_allocator;
    apollo_temp_alloc_f = options->temp_alloc;
    apollo_temp_realloc_f = options->temp_realloc;
    apollo_temp_free_f = options->temp_free;
    // Open mesh file
    FILE* file = fopen ( filename, "r" );

    if ( file == NULL ) {
        fprintf ( stderr, "Cannot open file %s", filename );
        return APOLLO_FILE_NOT_FOUND;
    }

    // Extract mesh file name
    size_t filename_len = strlen ( filename );
    const char* dir_end = filename + filename_len;

    while ( --dir_end != filename && *dir_end != '/' && *dir_end != '\\' )
        ;

    if ( dir_end != filename ) {
        ++dir_end;
    }

    strncpy ( model->name, dir_end, APOLLO_NAME_LEN );
    // Extract mesh file dir path
    assert ( filename_len < APOLLO_PATH_LEN );
    char base_dir[APOLLO_PATH_LEN];
    size_t base_dir_len = 0;
    base_dir[0] = '\0';

    if ( dir_end != filename ) {
        base_dir_len = dir_end - filename;
        strncpy ( base_dir, filename, base_dir_len );
    }

    // Allocated memory:
    //  speedup tables
    //  sparse pos/norm/tex data
    //  grouped pos/norm/tex data
    //  final ApolloModel
    // that means that for a model that takes up x memory, roughly 4x that memory amount is used.
    //
    // Temporary tables
    ApolloVertexTable vtable;
    ApolloAdjacencyTable atable;
    ApolloIndexTable itable;
    // Temporary buffers
    ApolloMaterial* materials = *_materials;
    ApolloTexture* textures = *_textures;
    ApolloFloat3* f_pos = NULL;
    ApolloFloat2* f_tex = NULL;
    ApolloFloat3* f_norm = NULL;
    size_t face_count = 0;
    typedef struct {
        uint32_t smoothing_group;
        uint32_t material;
        uint32_t indices_offset;
    } ApolloOBJMesh;
    ApolloOBJMesh* meshes = NULL;
    ApolloMaterial* used_materials = NULL;  // New ApolloMaterials go from material_libs to here while building, and from here to materials before returning.
    ApolloMaterialLib* material_libs = NULL;
    uint32_t* m_idx = NULL;
    typedef struct {
        ApolloFloat3 pos;
        ApolloFloat3 norm;
        ApolloFloat2 tex;
    } ApolloOBJVertex;
    ApolloOBJVertex* m_vert = NULL;
    // Prealloc memory
    apollo_vertex_table_create ( &vtable, 2 * APOLLO_PREALLOC_VERTEX_COUNT );
    apollo_adjacency_table_create ( &atable, 2 * APOLLO_PREALLOC_VERTEX_COUNT );
    apollo_index_table_create ( &itable, 2 * APOLLO_PREALLOC_INDEX_COUNT );
    sb_prealloc ( f_pos, APOLLO_PREALLOC_VERTEX_COUNT );
    sb_prealloc ( f_tex, APOLLO_PREALLOC_VERTEX_COUNT );
    sb_prealloc ( f_norm, APOLLO_PREALLOC_VERTEX_COUNT );
    sb_prealloc ( m_idx, APOLLO_PREALLOC_INDEX_COUNT );
    sb_prealloc ( m_vert, APOLLO_PREALLOC_VERTEX_COUNT );
    sb_prealloc ( meshes, APOLLO_PREALLOC_MESH_COUNT );
    sb_prealloc ( used_materials, APOLLO_PREALLOC_MESH_COUNT );
    sb_prealloc ( material_libs, APOLLO_PREALLOC_MESH_COUNT );
    // Start parsing
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

                char lib_name[APOLLO_PATH_LEN];
                memcpy ( lib_name, base_dir, base_dir_len );
                fscanf ( file, "%s", lib_name + base_dir_len );
                uint32_t idx = apollo_find_material_lib ( material_libs, lib_name );

                if ( idx == -1 ) {
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
                    ApolloOBJMesh* m = sb_add ( meshes, 1 );
                    m->indices_offset = sb_count ( m_idx );
                    m->material = -1;
                    m->smoothing_group = -1;
                }

                // Lookup the user materials
                uint32_t idx = apollo_find_material ( materials, mat_name );

                if ( idx != -1 ) {
                    sb_last ( meshes ).material = idx;
                    break;
                }

                // Lookup the additional materials that have been already loaded for this obj
                idx = apollo_find_material ( used_materials, mat_name );

                if ( idx != -1 ) {
                    sb_last ( meshes ).material = idx + sb_count ( materials );
                    break;
                }

                // Lookup the entire material libs that have been linked so far by this obj
                for ( size_t i = 0; i < sb_count ( material_libs ); ++i ) {
                    idx = apollo_find_material ( material_libs[i].materials, mat_name );

                    if ( idx != -1 ) {
                        sb_push ( used_materials, material_libs[i].materials[idx] );
                        sb_last ( meshes ).material = sb_count ( materials ) + sb_count ( used_materials ) - 1;
                        break;
                    }
                }

                if ( idx == -1 ) {
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
                        ApolloFloat3* pos = sb_add ( f_pos, 1 );

                        if ( !apollo_read_float3 ( file, pos ) ) {
                            result = APOLLO_INVALID_FORMAT;
                            goto error;
                        }

                        if ( options->flip_z ) {
                            pos->z = pos->z * -1.f;
                        }
                    }
                    break;

                    case 't': {
                        ApolloFloat2* uv = sb_add ( f_tex, 1 );

                        if ( !apollo_read_float2 ( file, uv ) ) {
                            result = APOLLO_INVALID_FORMAT;
                            goto error;
                        }

                        if ( options->flip_texcoord_v ) {
                            uv->y = 1.f - uv->y;
                        }
                    }
                    break;

                    case 'n': {
                        ApolloFloat3* norm = sb_add ( f_norm, 1 );

                        if ( !apollo_read_float3 ( file, norm ) ) {
                            result = APOLLO_INVALID_FORMAT;
                            goto error;
                        }

                        if ( options->flip_z ) {
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
                        ApolloOBJMesh* m = sb_add ( meshes, 1 );
                        m->indices_offset = sb_count ( m_idx );
                        break;
                }

                while ( fgetc ( file ) != '\n' )
                    ;

                break;

            case 's':
                x = fgetc ( file );
                x = fgetc ( file );

                if ( sb_count ( meshes ) == 0 ) {
                    ApolloOBJMesh* m = sb_add ( meshes, 1 );
                    m->indices_offset = sb_count ( m_idx );
                    m->material = -1;
                    m->smoothing_group = -1;
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
                            sb_push ( m_idx, first_vertex_index );
                            sb_push ( m_idx, last_vertex_index );
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
                        uint32_t* index = sb_add ( m_idx, 1 );

                        // In an OBJ file, the faces mark the end of a mesh.
                        // However it could be that the first mesh was implicitly started (not listed in the file), so we consider that case,
                        if ( sb_count ( meshes ) == 0 ) {
                            ApolloOBJMesh* m = sb_add ( meshes, 1 );
                            m->indices_offset = sb_count ( m_idx ) - 1;
                            m->material = -1;
                            m->smoothing_group = -1;
                        }

                        // Finalize the vertex, testing for overlaps if the mesh is smooth and updating the first/last face vertex indices
                        bool duplicate = false;

                        if ( sb_last ( meshes ).smoothing_group == 1 ) {
                            ApolloVertexTableItem* lookup = apollo_vertex_table_lookup ( &vtable, &f_pos[pos_index] );

                            if ( lookup != NULL ) {
                                *index = lookup->value;
                                duplicate = true;
                            }
                        }

                        if ( !duplicate ) {
                            // If it's not a duplicate, add it as a new vertex
                            ApolloOBJVertex* vertex = sb_add ( m_vert, 1 );
                            vertex->pos = f_pos[pos_index];
                            vertex->tex = tex_index != -1 ? f_tex[tex_index] : ApolloFloat2{ 0, 0 };
                            vertex->norm = norm_index != -1 ? f_norm[norm_index] : ApolloFloat3{ 0, 0, 0 };
                            *index = sb_count ( m_vert ) - 1;
                            ApolloVertexTableItem item = { vertex->pos, *index };
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
    if ( options->flip_faces ) {
        for ( size_t i = 0; i < sb_count ( m_idx ); i += 3 ) {
            uint32_t idx = m_idx[i];
            m_idx[i] = m_idx[i + 2];
            m_idx[i + 2] = idx;
        }
    }

    // The unnnecessary scoping is just so that it is possible to call goto error.
    {
        ApolloFloat3* face_normals = NULL;
        sb_add ( face_normals, face_count );
        ApolloFloat3* face_tangents = NULL;
        sb_add ( face_tangents, face_count );
        ApolloFloat3* face_bitangents = NULL;
        sb_add ( face_bitangents, face_count );

        // Compute face normals
        for ( size_t i = 0; i < face_count; ++i ) {
            ApolloFloat3 v0 = m_vert[m_idx[i * 3 + 0]].pos;
            ApolloFloat3 v1 = m_vert[m_idx[i * 3 + 1]].pos;
            ApolloFloat3 v2 = m_vert[m_idx[i * 3 + 2]].pos;
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
            size_t mesh_index_count = m < sb_count ( meshes ) - 1 ? meshes[m + 1].indices_offset - mesh_index_base : sb_count ( m_idx ) - mesh_index_base;
            size_t mesh_face_count = mesh_index_count / 3;

            if ( mesh_index_count % 3 != 0 ) {
                APOLLO_LOG_ERR ( "Malformed .obj file @ %s", filename );
                result = APOLLO_INVALID_FORMAT;
                goto error;
            }

            // If the mesh isn't smooth, skip it
            if ( meshes[m].smoothing_group == 0 ) {
                for ( size_t i = 0; i < mesh_face_count; ++i ) {
                    m_vert[m_idx[mesh_index_base + i * 3 + 0]].norm = face_normals[mesh_index_base / 3 + i];
                    m_vert[m_idx[mesh_index_base + i * 3 + 1]].norm = face_normals[mesh_index_base / 3 + i];
                    m_vert[m_idx[mesh_index_base + i * 3 + 2]].norm = face_normals[mesh_index_base / 3 + i];
                }

                continue;
            }

            // Vertex normals
            for ( size_t i = 0; i < mesh_index_count; ++i ) {
                ApolloFloat3 norm_sum;
                norm_sum.x = norm_sum.y = norm_sum.z = 0;
                // skip the index if the corresponding vertex has already been processed
                bool already_computed_vertex = apollo_index_table_lookup ( &itable, m_idx[mesh_index_base + i] );

                if ( already_computed_vertex ) {
                    continue;
                } else {
                    apollo_index_table_insert ( &itable, m_idx[mesh_index_base + i] );
                }

                ApolloAdjacencyTableItem* adj_lookup = apollo_adjacency_table_lookup ( &atable, m_idx[mesh_index_base + i] );

                for ( size_t j = 0; j < adj_lookup->count; ++j ) {
                    uint32_t k = adj_lookup->values[j];
                    norm_sum.x += face_normals[k].x;
                    norm_sum.y += face_normals[k].y;
                    norm_sum.z += face_normals[k].z;
                }

                ApolloAdjacencyTableExtension* ext = adj_lookup->next;

                while ( ext != NULL ) {
                    for ( size_t j = 0; j < ext->count; ++j ) {
                        uint32_t k = ext->values[j];
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
                m_vert[m_idx[mesh_index_base + i]].norm = normal;
            }

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
#define APOLLO_FINAL_MALLOC(T, n) (T*) options->final_alloc(options->final_allocator, sizeof(T) * n, 16);
        // Vertices
        model->vertex_data.pos_x = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.pos_y = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.pos_z = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.norm_x = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.norm_y = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.norm_z = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.tex_u = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_data.tex_v = APOLLO_FINAL_MALLOC ( float, sb_count ( m_vert ) );
        model->vertex_count = sb_count ( m_vert );

        for ( size_t i = 0; i < sb_count ( m_vert ); ++i ) {
            model->vertex_data.pos_x[i] = m_vert[i].pos.x;
            model->vertex_data.pos_y[i] = m_vert[i].pos.y;
            model->vertex_data.pos_z[i] = m_vert[i].pos.z;
            model->vertex_data.norm_x[i] = m_vert[i].norm.x;
            model->vertex_data.norm_y[i] = m_vert[i].norm.y;
            model->vertex_data.norm_z[i] = m_vert[i].norm.z;
            model->vertex_data.tex_u[i] = m_vert[i].tex.x;
            model->vertex_data.tex_v[i] = m_vert[i].tex.y;
        }

        // Meshes
        model->meshes = APOLLO_FINAL_MALLOC ( ApolloMesh, sb_count ( meshes ) );
        model->mesh_count = sb_count ( meshes );

        for ( size_t i = 0; i < sb_count ( meshes ); ++i ) {
            // Faces
            int mesh_index_base = meshes[i].indices_offset;
            size_t mesh_index_count = i < sb_count ( meshes ) - 1 ? meshes[i + 1].indices_offset - mesh_index_base : sb_count ( m_idx ) - mesh_index_base;
            int mesh_face_count = mesh_index_count / 3;

            if ( mesh_index_count % 3 != 0 ) {
                APOLLO_LOG_ERR ( "Malformed mesh file @ %s", filename );
                result = APOLLO_INVALID_FORMAT;
                goto error;
            }

            model->meshes[i].face_data.idx_a = APOLLO_FINAL_MALLOC ( uint32_t, mesh_face_count );
            model->meshes[i].face_data.idx_b = APOLLO_FINAL_MALLOC ( uint32_t, mesh_face_count );
            model->meshes[i].face_data.idx_c = APOLLO_FINAL_MALLOC ( uint32_t, mesh_face_count );
            model->meshes[i].face_data.normal_x = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.normal_y = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.normal_z = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.tangent_x = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.tangent_y = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.tangent_z = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.bitangent_x = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.bitangent_y = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_data.bitangent_z = APOLLO_FINAL_MALLOC ( float, mesh_face_count );
            model->meshes[i].face_count = mesh_face_count;

            for ( size_t j = 0; j < mesh_face_count; ++j ) {
                model->meshes[i].face_data.idx_a[j] = m_idx[mesh_index_base + j * 3 + 0];
                model->meshes[i].face_data.idx_b[j] = m_idx[mesh_index_base + j * 3 + 1];
                model->meshes[i].face_data.idx_c[j] = m_idx[mesh_index_base + j * 3 + 2];
                model->meshes[i].face_data.normal_x[j] = face_normals[mesh_index_base / 3 + j].x;
                model->meshes[i].face_data.normal_y[j] = face_normals[mesh_index_base / 3 + j].y;
                model->meshes[i].face_data.normal_z[j] = face_normals[mesh_index_base / 3 + j].z;
                model->meshes[i].face_data.tangent_x[j] = face_tangents[mesh_index_base / 3 + j].x;
                model->meshes[i].face_data.tangent_y[j] = face_tangents[mesh_index_base / 3 + j].y;
                model->meshes[i].face_data.tangent_z[j] = face_tangents[mesh_index_base / 3 + j].z;
                model->meshes[i].face_data.bitangent_x[j] = face_bitangents[mesh_index_base / 3 + j].x;
                model->meshes[i].face_data.bitangent_y[j] = face_bitangents[mesh_index_base / 3 + j].y;
                model->meshes[i].face_data.bitangent_z[j] = face_bitangents[mesh_index_base / 3 + j].z;
            }

            // Material
            model->meshes[i].material_id = meshes[i].material;
            // AABB
            ApolloFloat3 min = m_vert[m_idx[mesh_index_base]].pos;
            ApolloFloat3 max = m_vert[m_idx[mesh_index_base]].pos;

            for ( int j = 1; j < mesh_index_count; ++j ) {
                ApolloFloat3 pos = m_vert[m_idx[mesh_index_base + j]].pos;

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

            model->meshes[i].aabb_min[0] = min.x;
            model->meshes[i].aabb_min[1] = min.y;
            model->meshes[i].aabb_min[2] = min.z;
            model->meshes[i].aabb_max[0] = max.x;
            model->meshes[i].aabb_max[1] = max.y;
            model->meshes[i].aabb_max[2] = max.z;
            // Sphere
            ApolloFloat3 center;
            center.x = ( max.x + min.x ) / 2.f;
            center.y = ( max.y + min.y ) / 2.f;
            center.z = ( max.z + min.z ) / 2.f;
            float max_dist = 0;

            for ( size_t j = 1; j < mesh_index_count; ++j ) {
                ApolloFloat3 pos = m_vert[m_idx[mesh_index_base + j]].pos;
                ApolloFloat3 center_to_pos;
                center_to_pos.x = pos.x - center.x;
                center_to_pos.y = pos.y - center.y;
                center_to_pos.z = pos.z - center.z;
                float dist = sqrtf ( center_to_pos.x * center_to_pos.x + center_to_pos.y * center_to_pos.y + center_to_pos.z * center_to_pos.z );

                if ( dist > max_dist ) {
                    max_dist = dist;
                }
            }

            model->meshes[i].bounding_sphere[0] = center.x;
            model->meshes[i].bounding_sphere[1] = center.y;
            model->meshes[i].bounding_sphere[2] = center.z;
            model->meshes[i].bounding_sphere[3] = max_dist;
        }
    }
    // AABB & Sphere
    {
        ApolloFloat3 min = m_vert[0].pos;
        ApolloFloat3 max = m_vert[0].pos;

        for ( size_t j = 1; j < sb_count ( m_vert ); ++j ) {
            ApolloFloat3 pos = m_vert[j].pos;
            min.x = pos.x < min.x ? pos.x : min.x;
            min.y = pos.y < min.y ? pos.y : min.y;
            min.z = pos.z < min.z ? pos.z : min.z;
            max.x = pos.x > max.x ? pos.x : max.x;
            max.y = pos.y > max.y ? pos.y : max.y;
            max.z = pos.z > max.z ? pos.z : max.z;
        }

        model->aabb_min[0] = min.x;
        model->aabb_min[1] = min.y;
        model->aabb_min[2] = min.z;
        model->aabb_max[0] = max.x;
        model->aabb_max[1] = max.y;
        model->aabb_max[2] = max.z;
        //
        ApolloFloat3 center;
        center.x = ( max.x + min.x ) / 2.f;
        center.y = ( max.y + min.y ) / 2.f;
        center.z = ( max.z + min.z ) / 2.f;
        float max_dist = 0;

        for ( size_t j = 1; j < sb_count ( m_vert ); ++j ) {
            ApolloFloat3 pos = m_vert[j].pos;
            ApolloFloat3 center_to_pos;
            center_to_pos.x = pos.x - center.x;
            center_to_pos.y = pos.y - center.y;
            center_to_pos.z = pos.z - center.z;
            float dist = ( float ) sqrt ( center_to_pos.x * center_to_pos.x + center_to_pos.y * center_to_pos.y + center_to_pos.z * center_to_pos.z );

            if ( dist > max_dist ) {
                max_dist = dist;
            }
        }

        model->bounding_sphere[0] = center.x;
        model->bounding_sphere[1] = center.y;
        model->bounding_sphere[2] = center.z;
        model->bounding_sphere[3] = max_dist;
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
    sb_free ( f_pos );
    sb_free ( f_norm );
    sb_free ( f_tex );
    sb_free ( m_idx );
    sb_free ( m_vert );
    sb_free ( meshes );
    apollo_temp_allocator = NULL;
    apollo_temp_alloc_f = NULL;
    apollo_temp_realloc_f = NULL;
    apollo_temp_free_f = NULL;
    return APOLLO_SUCCESS;
error:
    apollo_vertex_table_destroy ( &vtable );
    apollo_adjacency_table_destroy ( &atable );
    apollo_index_table_destroy ( &itable );
    sb_free ( used_materials );
    sb_free ( material_libs );
    sb_free ( f_pos );
    sb_free ( f_norm );
    sb_free ( f_tex );
    sb_free ( m_idx );
    sb_free ( m_vert );
    sb_free ( meshes );
    apollo_temp_allocator = NULL;
    apollo_temp_alloc_f = NULL;
    apollo_temp_realloc_f = NULL;
    apollo_temp_free_f = NULL;
    return result;
#undef APOLLO_FINAL_MALLOC
}

//--------------------------------------------------------------------------------------------------
// IndexTable
//--------------------------------------------------------------------------------------------------
void apollo_index_table_create ( ApolloIndexTable* table, uint32_t capacity ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 ); // is power of 2
    table->items = APOLLO_TEMP_ALLOC ( uint32_t, capacity );
    memset ( table->items, 0, sizeof ( uint32_t ) * capacity );
    table->capacity = capacity;
    table->count = 0;
    table->access_mask = capacity - 1;
    table->is_zero_id_slot_used = false;
}

void apollo_index_table_destroy ( ApolloIndexTable* table ) {
    APOLLO_TEMP_FREE ( table->items, table->capacity );
    table->items = NULL;
}

void apollo_index_table_clear ( ApolloIndexTable* table ) {
    memset ( table->items, 0, sizeof ( uint32_t ) * table->capacity );
    table->is_zero_id_slot_used = false;
    table->count = 0;
}

uint64_t apollo_hash_u64 ( uint64_t h ) {
    // MurmerHash3
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

uint32_t apollo_hash_u32 ( uint32_t h ) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

uint32_t* apollo_index_table_get_item ( const ApolloIndexTable* table, uint32_t hash ) {
    return table->items + ( hash & table->access_mask );
}

uint32_t* apollo_index_table_get_next ( const ApolloIndexTable* table, uint32_t* item ) {
    if ( item + 1 != table->items + table->capacity ) {
        return item + 1;
    } else {
        return table->items;
    }
}

int apollo_index_table_item_distance ( const ApolloIndexTable* table, uint32_t* a, uint32_t* b ) {
    if ( b >= a ) {
        return b - a;
    } else {
        return table->capacity + b - a;
    }
}

void apollo_index_table_resize ( ApolloIndexTable* table, uint32_t new_capacity ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 ); // is power of 2
    uint32_t* old_items = table->items;
    uint32_t old_capacity = table->capacity;
    table->items = APOLLO_TEMP_ALLOC ( uint32_t, new_capacity );
    memset ( table->items, 0, sizeof ( uint32_t ) * new_capacity );
    table->capacity = new_capacity;
    table->access_mask = new_capacity - 1;

    for ( uint32_t* old_item = old_items; old_item != old_items + old_capacity; ++old_item ) {
        if ( *old_item != 0 ) { // is used
            uint32_t hash = apollo_hash_u32 ( *old_item );
            uint32_t* new_item = apollo_index_table_get_item ( table, hash );

            while ( *new_item != 0 ) { // find an unused slot
                new_item = apollo_index_table_get_next ( table, new_item );
            }

            *new_item = *old_item;
        }
    }

    APOLLO_TEMP_FREE ( old_items, old_capacity );
}

void apollo_index_table_insert ( ApolloIndexTable* table, uint32_t item ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_index_table_resize ( table, table->capacity * 2 );
    }

    if ( item != 0 ) {
        uint32_t hash = apollo_hash_u32 ( item );
        uint32_t* dest = apollo_index_table_get_item ( table, hash );

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

bool apollo_index_table_lookup ( ApolloIndexTable* table, uint32_t item ) {
    if ( item != 0 ) {
        uint32_t hash = apollo_hash_u32 ( item );
        uint32_t* slot = apollo_index_table_get_item ( table, hash );

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
void apollo_vertex_table_create ( ApolloVertexTable* table, uint32_t capacity ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 ); // is power of 2
    table->items = APOLLO_TEMP_ALLOC ( ApolloVertexTableItem, capacity );
    memset ( table->items, 0, sizeof ( *table->items ) * capacity );
    table->capacity = capacity;
    table->count = 0;
    table->access_mask = capacity - 1;
    table->is_zero_key_item_used = false;
    table->zero_key_item = { { 0, 0, 0 }, 0 };
}

void apollo_vertex_table_destroy ( ApolloVertexTable* table ) {
    APOLLO_TEMP_FREE ( table->items, table->capacity );
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

void apollo_vertex_table_resize ( ApolloVertexTable* table, uint32_t new_capacity ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 ); // is power of 2
    ApolloVertexTableItem* old_items = table->items;
    uint32_t old_capacity = table->capacity;
    table->items = APOLLO_TEMP_ALLOC ( ApolloVertexTableItem, new_capacity );
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

    APOLLO_TEMP_FREE ( old_items, old_capacity );
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
            item->next = ( ApolloAdjacencyTableExtension* ) apollo_temp_alloc_f ( apollo_temp_allocator, sizeof ( ApolloAdjacencyTableExtension ), 64 );
            memset ( item->next, 0, sizeof ( *item->next ) );
            ext = item->next;
        }
    }

    if ( ext->count == APOLLO_ADJACENCY_EXTENSION_CAPACITY ) {
        ext->next = ( ApolloAdjacencyTableExtension* ) apollo_temp_alloc_f ( apollo_temp_allocator, sizeof ( ApolloAdjacencyTableExtension ), 64 );
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

void apollo_adjacency_table_create ( ApolloAdjacencyTable* table, uint32_t capacity ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 );
    table->count = 0;
    table->capacity = capacity;
    table->access_mask = capacity - 1;
    table->items = APOLLO_TEMP_ALLOC ( ApolloAdjacencyTableItem, capacity );
    memset ( table->items, 0, sizeof ( ApolloAdjacencyTableItem ) * capacity );
    memset ( &table->zero_key_item, 0, sizeof ( table->zero_key_item ) );
}

void apollo_adjacency_table_destroy ( ApolloAdjacencyTable* table ) {
    for ( size_t i = 0; i < table->capacity; ++i ) {
        ApolloAdjacencyTableExtension* ext = table->items[i].next;

        while ( ext ) {
            ApolloAdjacencyTableExtension* p = ext;
            ext = ext->next;
            APOLLO_TEMP_FREE ( p, 1 );
        }
    }

    APOLLO_TEMP_FREE ( table->items, table->capacity );
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

void apollo_adjacency_table_resize ( ApolloAdjacencyTable* table, uint32_t new_capacity ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 );
    ApolloAdjacencyTableItem* old_items = table->items;
    uint32_t old_capacity = table->capacity;
    table->items = APOLLO_TEMP_ALLOC ( ApolloAdjacencyTableItem, new_capacity );
    memset ( table->items, 0, sizeof ( ApolloAdjacencyTableItem ) * new_capacity );
    table->capacity = new_capacity;

    for ( ApolloAdjacencyTableItem* old_item = old_items; old_item != old_items + old_capacity; ++old_item ) {
        uint32_t hash = apollo_hash_u32 ( old_item->key );
        ApolloAdjacencyTableItem* new_item = apollo_adjacency_table_get_item ( table, hash );

        while ( new_item->key != 0 ) {
            new_item = apollo_adjacency_table_get_next ( table, new_item );
        }

        *new_item = *old_item;
    }

    APOLLO_TEMP_FREE ( old_items, old_capacity );
}

ApolloAdjacencyTableItem* apollo_adjacency_table_insert ( ApolloAdjacencyTable* table, uint32_t key ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_adjacency_table_resize ( table, table->capacity * 2 );
    }

    if ( key != 0 ) {
        uint32_t hash = apollo_hash_u32 ( key );
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

ApolloAdjacencyTableItem* apollo_adjacency_table_lookup ( ApolloAdjacencyTable* table, uint32_t key ) {
    if ( key != 0 ) {
        uint32_t hash = apollo_hash_u32 ( key );
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
    int* p = arr ? ( int* ) apollo_temp_realloc_f ( apollo_temp_allocator, stb__sbraw ( arr ), stb__sbm ( arr ) * itemsize, itemsize * m + sizeof ( int ) * 2, itemsize >= 4 ? itemsize : 4 )
             : ( int* ) apollo_temp_alloc_f ( apollo_temp_allocator, itemsize * m + sizeof ( int ) * 2, itemsize >= 4 ? itemsize : 4 );

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