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

#define APOLLO_INVALID_ID ((uint32_t)-1)
#define APOLLO_PATH_LEN 1024
#define APOLLO_NAME_LEN 256

typedef struct ApolloMeshFaceData {
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

typedef struct ApolloMesh {
    ApolloMeshFaceData face_data;
    uint32_t    face_count;
    uint32_t    material_id;
    float       aabb_min[3];
    float       aabb_max[3];
    float       bounding_sphere[4];   // <x y z r>
    char        name[APOLLO_NAME_LEN];
} ApolloMesh;

typedef struct ApolloModelVertexData {
    float* pos_x;
    float* pos_y;
    float* pos_z;
    float* norm_x;
    float* norm_y;
    float* norm_z;
    float* tex_u;
    float* tex_v;
} ApolloModelVertexData;

typedef struct ApolloModel {
    ApolloModelVertexData vertex_data;
    uint32_t    vertex_count;
    uint32_t    mesh_count;
    float       aabb_min[3];
    float       aabb_max[3];
    float       bounding_sphere[4];   // <x y z r>
    ApolloMesh* meshes;
    char        name[APOLLO_NAME_LEN];
    char        dir[APOLLO_NAME_LEN];
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
    APOLLO_PBR,
    APOLLO_DISNEY,
    APOLLO_BSDF_COUNT,
    APOLLO_BSDF_INVALID = APOLLO_BSDF_COUNT
} ApolloBSDF;

// Path-tracer subset of http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr
typedef struct ApolloMaterial {
    float       ior;                    // Ni
    float       albedo[3];              // Kd
    uint32_t    albedo_texture;         // map_Kd
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
    float       sheen;                  // Ps
    float       sheen_tint;             // Pst
    float       specular_tint;          // Kst
    float       clearcoat;              // Pc
    float       clearcoat_gloss;        // Pcg
    float       subsurface;             // Ss
    float       anisotropic;            // aniso
    ApolloBSDF  bsdf;                   // illum
    char        name[APOLLO_NAME_LEN];
} ApolloMaterial;

typedef struct {
    float albedo[3];
    uint32_t albedo_texture;
} ApolloBSDFParamsDiffuse;

typedef struct {
    float albedo[3];
    float specular[3];
    float specular_exp;
    uint32_t albedo_texture;
    uint32_t specular_texture;
} ApolloBSDFParamsSpecular;

typedef struct {
    float ior;
    float metalness;
    float roughness;
    uint32_t metalness_texture;
    uint32_t roughness_texture;
} ApolloBSDFParamsPBR;

typedef struct {
    float metalness;
    float roughness;
    float specular;
    float specular_tint;
    float sheen;
    float sheen_tint;
    float clearcoat;
    float clearcoat_gloss;
    float subsurface;
    float anisotropic;
    uint32_t base_color_texture;
    uint32_t metalness_texture;
    uint32_t roughness_texture;
    uint32_t specular_texture;
    uint32_t specular_tint_texture;
    uint32_t sheen_texture;
    uint32_t sheen_tint_texture;
    uint32_t clearcoat_texture;
    uint32_t clearcoat_gloss_texture;
    uint32_t subsurface_texture;
    uint32_t anisotropic_texture;
} ApolloBSDFParamsDisney;

typedef struct {
    float albedo[3];
    float emissive[3];
    uint32_t albedo_texture;
    uint32_t emissive_texture;
    uint32_t normal_texture;
    uint32_t bump_texture;
    uint32_t displacement_texture;
} ApolloMaterialCommonParams;

typedef struct ApolloMaterial2 {
    char name[APOLLO_NAME_LEN];
    ApolloBSDF bsdf;
    union {
        ApolloBSDFParamsDiffuse diffuse;
        ApolloBSDFParamsSpecular specular;
        ApolloBSDFParamsPBR pbr;
        ApolloBSDFParamsDisney disney;
    } BSDF_params;
    ApolloMaterialCommonParams common_params;
} ApolloMaterial2;

#define APOLLO_TEXTURE_NONE UINT32_MAX

typedef struct ApolloTexture {
    char name[APOLLO_NAME_LEN];
} ApolloTexture;

typedef void* ( apollo_alloc_fun ) ( void* allocator, size_t size, size_t alignment );
typedef void* ( apollo_realloc_fun ) ( void* allocator, void* addr, size_t old_size, size_t new_size, size_t new_alignment );
typedef void ( apollo_free_fun ) ( void* allocator, void* addr, size_t size );

typedef struct {
    void* p;
    apollo_alloc_fun* alloc;
    apollo_realloc_fun* realloc;
    apollo_free_fun* free;
} ApolloAllocator;

typedef struct ApolloLoadOptions {
    ApolloAllocator temp_allocator;
    ApolloAllocator final_allocator;
    bool flip_z;
    bool flip_texcoord_v;
    bool flip_faces_winding_order;
    bool recompute_vertex_normals;
    bool compute_face_normals;
    bool remove_vertex_duplicates;
    bool compute_tangents;
    bool compute_bitangents;
    bool compute_bounds;
    size_t prealloc_vertex_count;
    size_t prealloc_index_count;
    size_t prealloc_mesh_count;
} ApolloLoadOptions;

// The provided allocator is not required to strictly follow the alignment requirements. A malloc-based allocator is fine.
// If it does, the output data arrays are guaranteed to be 16-byte aligned.
ApolloResult apollo_import_model_obj ( const char* filename, ApolloModel* model, ApolloMaterial** materials, ApolloTexture** textures, ApolloLoadOptions* options );

void apollo_buffer_free ( void* p, ApolloAllocator* allocator );
int apollo_buffer_size ( void* p );

// Very basic model dump to obj. .obj and .mtl files named after the model name will be created in base_path folder.
void apollo_dump_model_obj ( const ApolloModel* model, const ApolloMaterial* materials, const ApolloTexture* textures, const char* base_path );

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

#ifndef APOLLO_LOG_VERBOSE
#ifdef APOLLO_LOG_VERBOSE_ENABLE
#define APOLLO_LOG_VERBOSE(fmt, ...) fprintf(stdout, "Apollo Verbose: " fmt "\n", __VA_ARGS__)
#else
#define APOLLO_LOG_VERBOSE(...)
#endif
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

ApolloResult    apollo_read_texture ( FILE* file, const ApolloTexture* texture_bank, ApolloTexture* new_textures, uint32_t* new_texture_idx_out, ApolloAllocator* new_texture_allocator );
bool            apollo_read_float2 ( FILE* file, ApolloFloat2* val );
bool            apollo_read_float3 ( FILE* file, ApolloFloat3* val );

// Material Libraries are files containing one or more material definitons.
// Model files like obj shall then reference those materials by name.
typedef struct {
    FILE* file;
    ApolloMaterial* materials;
    char filename[APOLLO_PATH_LEN];
} ApolloMaterialLib;

// Parses an entire .mtl file into a materials array. All referenced textures are added to the textures array. For both cases the given allocator is used.
ApolloResult    apollo_open_material_lib ( const char* filename, ApolloMaterialLib* lib, const ApolloTexture* texture_bank, ApolloTexture* new_textures, ApolloAllocator* allocator );
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

void            apollo_index_table_create ( ApolloIndexTable* table, uint32_t capacity, ApolloAllocator* allocator );
void            apollo_index_table_destroy ( ApolloIndexTable* table, ApolloAllocator* allocator );
void            apollo_index_table_clear ( ApolloIndexTable* table );
uint32_t        apollo_hash_u32 ( uint32_t key );
uint64_t        apollo_hash_u64 ( uint64_t key );
uint32_t*       apollo_index_table_get_item ( const ApolloIndexTable* table, uint32_t hash );
uint32_t*       apollo_index_table_get_next ( const ApolloIndexTable* table, uint32_t* item );
int             apollo_index_table_item_distance ( const ApolloIndexTable* table, uint32_t* a, uint32_t* b );
void            apollo_index_table_resize ( ApolloIndexTable* table, uint32_t new_capacity, ApolloAllocator* allocator );
void            apollo_index_table_insert ( ApolloIndexTable* table, uint32_t item, ApolloAllocator* allocator );
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

void                    apollo_vertex_table_create ( ApolloVertexTable* table, uint32_t capacity, ApolloAllocator* allocator );
void                    apollo_vertex_table_destroy ( ApolloVertexTable* table, ApolloAllocator* allocator );
uint32_t                apollo_f3_hash ( const ApolloFloat3* v );
ApolloVertexTableItem*  apollo_vertex_table_get_item ( const ApolloVertexTable* table, uint32_t hash );
ApolloVertexTableItem*  apollo_vertex_table_get_next ( const ApolloVertexTable* table, ApolloVertexTableItem* item );
int                     apollo_vertex_table_item_distance ( const ApolloVertexTable* table, ApolloVertexTableItem* a, ApolloVertexTableItem* b );
void                    apollo_vertex_table_resize ( ApolloVertexTable* table, uint32_t new_capacity, ApolloAllocator* allocator );
void                    apollo_vertex_table_insert ( ApolloVertexTable* table, const ApolloVertexTableItem* item, ApolloAllocator* allocator );
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
bool        apollo_adjacency_item_add_unique ( ApolloAdjacencyTableItem* item, uint32_t value, ApolloAllocator* allocator );
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

void                        apollo_adjacency_table_create ( ApolloAdjacencyTable* table, uint32_t capacity, ApolloAllocator* allocator );
void                        apollo_adjacency_table_destroy ( ApolloAdjacencyTable* table, ApolloAllocator* allocator );
ApolloAdjacencyTableItem*   apollo_adjacency_table_get_item ( ApolloAdjacencyTable* table, uint32_t hash );
ApolloAdjacencyTableItem*   apollo_adjacency_table_get_next ( ApolloAdjacencyTable* table, ApolloAdjacencyTableItem* item );
int                         apollo_adjacency_table_distance ( const ApolloAdjacencyTable* table, const ApolloAdjacencyTableItem* a, const ApolloAdjacencyTableItem* b );
void                        apollo_adjacency_table_resize ( ApolloAdjacencyTable* table, uint32_t new_capacity, ApolloAllocator* allocator );
ApolloAdjacencyTableItem*   apollo_adjacency_table_insert ( ApolloAdjacencyTable* table, uint32_t key, ApolloAllocator* allocator );
ApolloAdjacencyTableItem*   apollo_adjacency_table_lookup ( ApolloAdjacencyTable* table, uint32_t key );

#define APOLLO_ALLOC_ALIGN(allocator, T, n, align) (T*) (allocator)->alloc(allocator, sizeof(T) * n, align)
#define APOLLO_ALLOC(allocator, T, n, align) APOLLO_ALLOC_ALIGN(allocator, T, n, alignof(T))
// Waiting for the day MSVC C compiler will support typeof()...
#define APOLLO_REALLOC_ALIGN(allocator, T, p, old_n, new_n, align) (T*) (allocator)->alloc(allocator, p, sizeof(T) * old_n, sizeof(T) * new_n, align)
#define APOLLO_REALLOC(allocator, T, p, old_n, new_n, align) APOLLO_REALLOC_ALIGN(allocator, T, p, old_n, new_n, alignof(T))
#define APOLLO_FREE(allocator, p, n) (allocator)->free(allocator, p, sizeof(*p) * n)

// Custom version of https://github.com/nothings/stb/blob/master/stretchy_buffer.h
// Depends on apollo temp allocators
#define apollo_sb_free(a,allocator)         ((a) ? (allocator).free((allocator).p,apollo__sbraw(a),apollo__sbm(a)),0 : 0)
#define apollo_sb_push(a,v,allocator)       (apollo__sbmaybegrow(a,1,(allocator)), (a)[apollo__sbn(a)++] = (v))
#define apollo_sb_count(a)                  ((a) ? apollo__sbn(a) : 0)
#define apollo_sb_add(a,n,allocator)        (apollo__sbmaybegrow(a,n,(allocator)), apollo__sbn(a)+=(n), &(a)[apollo__sbn(a)-(n)])
#define apollo_sb_last(a)                   ((a)[apollo__sbn(a)-1])
#define apollo_sb_prealloc(a,n,allocator)   (apollo__sbgrow(a,n,(allocator)))

#define apollo__sbraw(a)           ((int *) (a) - 2)
#define apollo__sbm(a)             apollo__sbraw(a)[0]
#define apollo__sbn(a)             apollo__sbraw(a)[1]

#define apollo__sbneedgrow(a,n)             ((a)==0 || apollo__sbn(a)+(n) >= apollo__sbm(a))
#define apollo__sbmaybegrow(a,n,allocator)  (apollo__sbneedgrow(a,(n)) ? apollo__sbgrow(a,n,allocator) : 0)
#define apollo__sbgrow(a,n,allocator)       (*((void **)&(a)) = apollo__sbgrowf((a), (n), sizeof(*(a)), &allocator))

void* apollo__sbgrowf ( void* arr, int increment, int itemsize, ApolloAllocator* allocator );

//--------------------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------------------
uint32_t apollo_find_texture ( const ApolloTexture* textures, const char* texture_name ) {
    for ( size_t i = 0; i < apollo_sb_count ( textures ); ++i ) {
        if ( strcmp ( texture_name, textures[i].name ) == 0 ) {
            return i;
        }
    }

    return -1;
}

uint32_t apollo_find_material ( const ApolloMaterial* materials, const char* mat_name ) {
    for ( size_t i = 0; i < apollo_sb_count ( materials ); ++i ) {
        if ( strcmp ( mat_name, materials[i].name ) == 0 ) {
            return i;
        }
    }

    return -1;
}

uint32_t apollo_find_material_lib ( const ApolloMaterialLib* libs, const char* lib_filename ) {
    for ( size_t i = 0; i < apollo_sb_count ( libs ); ++i ) {
        if ( strcmp ( lib_filename, libs[i].filename ) == 0 ) {
            return i;
        }
    }

    return -1;
}

void apollo_buffer_free ( void* p, ApolloAllocator* allocator ) {
    apollo_sb_free ( p, *allocator );
}

int apollo_buffer_size ( void* p ) {
    return apollo_sb_count ( p );
}


//--------------------------------------------------------------------------------------------------
// File parsing routines
//--------------------------------------------------------------------------------------------------
// TODO this should read the texture data instead of just storing the name...
ApolloResult apollo_read_texture ( FILE* file, const ApolloTexture* texture_bank, ApolloTexture* new_textures, uint32_t* new_texture_idx_out, ApolloAllocator* new_texture_allocator ) {
    char name[APOLLO_NAME_LEN];

    if ( fscanf ( file, "%s", name ) != 1 ) {
        return APOLLO_INVALID_FORMAT;
    }

    uint32_t idx = apollo_find_texture ( texture_bank, name );

    if ( idx == -1 ) {
        idx = apollo_sb_count ( new_textures );
        ApolloTexture* texture = apollo_sb_add ( new_textures, 1, *new_texture_allocator );
        strncpy ( texture->name, name, 256 );
    }

    *new_texture_idx_out = idx;
    return APOLLO_SUCCESS;
}

bool apollo_read_float ( FILE* file, float* val ) {
#if 1
    return fscanf ( file, "%f", val ) > 0;
#else
    char str[64];

    if ( fscanf ( file, "%s", str ) != 1 ) {
        return false;
    }

    // Parse sign
    const char* p = str;
    char sign = '+';

    if ( *p == '+' || *p == '-' ) {
        sign = *p;
        ++p;
    }

    // Read int part
    double mantissa = 0.0;

    while ( *p != 'e' && *p != 'E' && *p != '.' && *p != ' ' && *p != '\0' ) {
        mantissa *= 10.0;
        mantissa += *p - '0';
        ++p;
    }

    // Read decimal part
    if ( *p == '.' ) {
        ++p;
        double frac = 1.0;

        while ( *p != 'e' && *p != 'E' && *p != ' ' && *p != '\0' ) {
            frac *= 0.1;
            mantissa += ( *p - '0' ) * frac;
            ++p;
        }
    }

    // Read exp
    int exp = 0;
    char exp_sign = '+';

    if ( *p == 'e' || *p == 'E' ) {
        ++p;

        // Read exp sign
        if ( *p == '+' || *p == '-' ) {
            exp_sign = *p;
            ++p;
        }

        // Read exp value
        while ( *p != ' ' && *p != '\0' ) {
            exp *= 10;
            exp += *p - '0';
            ++p;
        }
    }

    // Assemble
    double a = pow ( 5, exp );
    double b = pow ( 2, exp );

    if ( exp_sign == '-' ) {
        a = 1.0 / a;
        b = 1.0 / b;
    }

    *val = ( float ) ( ( sign == '+' ? 1.0 : -1.0 ) * mantissa * a * b );
    return true;
#endif
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
    mat->name[0] = '\0';
    mat->bsdf = APOLLO_BSDF_INVALID;
    mat->bump_texture = APOLLO_TEXTURE_NONE;
    mat->albedo_texture = APOLLO_TEXTURE_NONE;
    mat->displacement_texture = APOLLO_TEXTURE_NONE;
    mat->emissive_texture = APOLLO_TEXTURE_NONE;
    mat->metallic_texture = APOLLO_TEXTURE_NONE;
    mat->normal_texture = APOLLO_TEXTURE_NONE;
    mat->roughness_texture = APOLLO_TEXTURE_NONE;
    mat->specular_texture = APOLLO_TEXTURE_NONE;
    mat->specular_exp_texture = APOLLO_TEXTURE_NONE;
    mat->name[0] = '\0';
}

ApolloResult apollo_open_material_lib ( const char* filename, ApolloMaterialLib* lib, const ApolloTexture* texture_bank, ApolloTexture* new_textures, ApolloAllocator* allocator ) {
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

            for ( size_t i = 0; i < apollo_sb_count ( materials ); ++i ) {
                if ( strcmp ( mat_name, materials[i].name ) == 0 ) {
                    APOLLO_LOG_ERR ( "Duplicate material name (%s) in materials library %s\n", mat_name, filename );
                    goto error;
                }
            }

            size_t mat_name_len = strlen ( mat_name );
            ApolloMaterial* mat = apollo_sb_add ( materials, 1, *allocator );
            apollo_material_clear ( mat );
            strcpy ( mat->name, mat_name );
        } else if ( strcmp ( key, "Ni" ) == 0 ) {
            float Ni;

            if ( !apollo_read_float ( file, &Ni ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ni on file %s\n", filename );
                goto error;
            }
        }
        // Albedo
        else if ( strcmp ( key, "Kd" ) == 0 ) {
            ApolloFloat3 Kd;

            if ( !apollo_read_float3 ( file, &Kd ) ) {
                APOLLO_LOG_ERR ( "Format error reading Kd on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).albedo[0] = Kd.x;
            apollo_sb_last ( materials ).albedo[1] = Kd.y;
            apollo_sb_last ( materials ).albedo[2] = Kd.z;
        } else if ( strcmp ( key, "map_Kd" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).albedo_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading albedo texture on file %s\n", idx, filename );
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

            apollo_sb_last ( materials ).specular[0] = Ks.x;
            apollo_sb_last ( materials ).specular[1] = Ks.y;
            apollo_sb_last ( materials ).specular[2] = Ks.z;
        } else if ( strcmp ( key, "map_Ks" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).specular_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading specular texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Specular exponent
        else if ( strcmp ( key, "Ns" ) == 0 ) {
            float Ns;

            if ( !apollo_read_float ( file, &Ns ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ns on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).specular_exp = Ns;
        }
        // Bump mapping
        else if ( strcmp ( key, "bump" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).bump_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading bump texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Roughness
        else if ( strcmp ( key, "Pr" ) == 0 ) {
            float Pr;

            if ( !apollo_read_float ( file, &Pr ) ) {
                APOLLO_LOG_ERR ( "Format error reading Pr on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).roughness = Pr;
        } else if ( strcmp ( key, "map_Pr" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).roughness_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading roughness texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Metallic
        else if ( strcmp ( key, "Pm" ) == 0 ) {
            float Pm;

            if ( !apollo_read_float ( file, &Pm ) ) {
                APOLLO_LOG_ERR ( "Format error reading Pm on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).metallic = Pm;
        } else if ( strcmp ( key, "map_Pm" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).metallic_texture = idx;
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

            apollo_sb_last ( materials ).emissive[0] = Ke.x;
            apollo_sb_last ( materials ).emissive[1] = Ke.y;
            apollo_sb_last ( materials ).emissive[2] = Ke.z;
        } else if ( strcmp ( key, "map_Ke" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).emissive_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading emissive texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Normal map
        else if ( strcmp ( key, "normal" ) == 0 ) {
            uint32_t idx;
            ApolloResult result = apollo_read_texture ( file, texture_bank, new_textures, &idx, allocator );

            if ( result == APOLLO_SUCCESS ) {
                apollo_sb_last ( materials ).normal_texture = idx;
            } else {
                APOLLO_LOG_ERR ( "Error %d(if ==-1 format error; if== -2 texture error)reading normal texture on file %s\n", idx, filename );
                goto error;
            }
        }
        // Sheen
        else if ( strcmp ( key, "Ps" ) == 0 ) {
            float Ps;

            if ( !apollo_read_float ( file, &Ps ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ps on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).sheen = Ps;
        }
        // Sheen tint
        else if ( strcmp ( key, "Pst" ) == 0 ) {
            float Pst;

            if ( !apollo_read_float ( file, &Pst ) ) {
                APOLLO_LOG_ERR ( "Format error reading Pst on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).sheen_tint = Pst;
        }
        // Specular tint
        else if ( strcmp ( key, "Kst" ) == 0 ) {
            float Kst;

            if ( !apollo_read_float ( file, &Kst ) ) {
                APOLLO_LOG_ERR ( "Format error reading Kst on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).specular_tint = Kst;
        }
        // Clearcoat
        else if ( strcmp ( key, "Pc" ) == 0 ) {
            float Pc;

            if ( !apollo_read_float ( file, &Pc ) ) {
                APOLLO_LOG_ERR ( "Format error reading Pc on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).clearcoat = Pc;
        }
        // Clearcoat Gloss
        else if ( strcmp ( key, "Pcg" ) == 0 ) {
            float Pcg;

            if ( !apollo_read_float ( file, &Pcg ) ) {
                APOLLO_LOG_ERR ( "Format error reading Pcg on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).clearcoat_gloss = Pcg;
        }
        // Subsurface
        else if ( strcmp ( key, "Ss" ) == 0 ) {
            float Ss;

            if ( !apollo_read_float ( file, &Ss ) ) {
                APOLLO_LOG_ERR ( "Format error reading Ss on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).subsurface = Ss;
        }
        // Anisotropic
        else if ( strcmp ( key, "aniso" ) == 0 ) {
            float aniso;

            if ( !apollo_read_float ( file, &aniso ) ) {
                APOLLO_LOG_ERR ( "Format error reading aniso on file %s\n", filename );
                goto error;
            }

            apollo_sb_last ( materials ).anisotropic = aniso;
        }
        // BSDF
        else if ( strcmp ( key, "illum" ) == 0 ) {
            char illum[256];

            if ( fscanf ( file, "%s", &illum ) != 1 ) {
                APOLLO_LOG_ERR ( "Failed to read illum on file %s", filename );
                goto error;
            }

            if ( strcmp ( illum, "diffuse" ) == 0 ) {
                apollo_sb_last ( materials ).bsdf = APOLLO_DIFFUSE;
            } else if ( strcmp ( illum, "specular" ) == 0 ) {
                apollo_sb_last ( materials ).bsdf = APOLLO_SPECULAR;
            } else if ( strcmp ( illum, "mirror" ) == 0 ) {
                apollo_sb_last ( materials ).bsdf = APOLLO_MIRROR;
            } else if ( strcmp ( illum, "pbr" ) == 0 ) {
                apollo_sb_last ( materials ).bsdf = APOLLO_PBR;
            } else if ( strcmp ( illum, "disney" ) == 0 ) {
                apollo_sb_last ( materials ).bsdf = APOLLO_DISNEY;
            } else {
                APOLLO_LOG_ERR ( "Unsupported material bsdf %s", illum );
            }
        } else if ( strcmp ( key, "#" ) == 0 ) {
            while ( getc ( file ) != '\n' );
        } else if ( strcmp ( key, "map_d" ) == 0 ) {
            while ( getc ( file ) != '\n' );
        } else if ( strcmp ( key, "map_Ka" ) == 0 ) {
            while ( getc ( file ) != '\n' );
        } else if ( strcmp ( key, "d" ) == 0 || strcmp ( key, "Tr" ) == 0 || strcmp ( key, "Tf" ) == 0 || strcmp ( key, "Ka" ) == 0 ) {
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
typedef struct {
    uint32_t smoothing_group;
    uint32_t material;
    uint32_t indices_offset;
    char name[APOLLO_NAME_LEN];
} ApolloOBJMesh;

void apollo_initialize_mesh_obj ( ApolloOBJMesh* mesh ) {
    mesh->smoothing_group = APOLLO_INVALID_ID;
    mesh->material = APOLLO_INVALID_ID;
    mesh->indices_offset = 0;
    mesh->name[0] = '\0';
}

// Removes submeshes which have invalid materials
//--------------------------------------------------------------------------------------------------
void apollo_filter_invalid_models ( ApolloModel* model ) {
    APOLLO_LOG_VERBOSE ( "mesh_count %d\n", model->mesh_count );
    uint32_t i;

    for ( i = 0; i < model->mesh_count; ) {
        ApolloMesh* mesh = model->meshes + i;
        APOLLO_LOG_VERBOSE ( "mesh %d", i );
        APOLLO_LOG_VERBOSE ( "name %s", mesh->name );
        APOLLO_LOG_VERBOSE ( "material_id %u", mesh->material_id );
        APOLLO_LOG_VERBOSE ( "face_count %u", mesh->face_count );

        if ( mesh->material_id == APOLLO_INVALID_ID ) {
            // This could also be caused by an invalid command order, but we probably don't want to remove invalid meshes
            assert ( mesh->face_count == 0 );
            memcpy ( mesh, model->meshes + ( model->mesh_count - 1 ), sizeof ( ApolloMesh ) );
            --model->mesh_count;
            APOLLO_LOG_VERBOSE ( "Removing mesh" );
        } else {
            ++i;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Model
//--------------------------------------------------------------------------------------------------
ApolloResult apollo_import_model_obj ( const char* filename, ApolloModel* model, ApolloMaterial** materials_bank, ApolloTexture** textures_bank, ApolloLoadOptions* options ) {
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

    const char* name_end = dir_end;

    while ( *(++name_end) != '\0' && *name_end != '.')
        ;

    strncpy ( model->name, dir_end, name_end - dir_end );
    model->name[name_end - dir_end] = '\0';
    // Extract mesh file dir path
    assert ( filename_len < APOLLO_PATH_LEN );
    char base_dir[APOLLO_PATH_LEN];
    size_t base_dir_len = 0;
    base_dir[0] = '\0';

    if ( dir_end != filename ) {
        base_dir_len = dir_end - filename;
        strncpy ( base_dir, filename, base_dir_len );
    }

    strncpy ( model->dir, base_dir, base_dir_len );
    model->dir[base_dir_len] = '\0';
    // Allocated memory:
    //  speedup tables
    //  sparse pos/norm/tex data
    //  grouped pos/norm/tex data
    //  final ApolloModel
    // that means that for a model that takes up x memory, roughly 4x that memory amount is used.
    //
    // Temporary tables
    ApolloVertexTable vtable = {};
    ApolloAdjacencyTable atable = {};
    ApolloIndexTable itable = {};
    // Temporary buffers
    ApolloFloat3* f_pos = NULL;
    ApolloFloat2* f_tex = NULL;
    ApolloFloat3* f_norm = NULL;
    size_t face_count = 0;
    ApolloOBJMesh* meshes = NULL;
    ApolloTexture* new_textures = NULL;
    ApolloMaterial* new_materials = NULL;
    ApolloMaterialLib* material_libs = NULL; // Temporary storage where .mtl files get loaded into RAM before being processed
    uint32_t* m_idx = NULL;
    typedef struct {
        ApolloFloat3 pos;
        ApolloFloat3 norm;
        ApolloFloat2 tex;
    } ApolloOBJVertex;
    ApolloOBJVertex* m_vert = NULL;

    // Prealloc memory
    if ( options->remove_vertex_duplicates ) {
        apollo_vertex_table_create ( &vtable, 2 * options->prealloc_vertex_count, &options->temp_allocator );
    }

    if ( options->recompute_vertex_normals ) {
        apollo_adjacency_table_create ( &atable, 2 * options->prealloc_vertex_count, &options->temp_allocator );
        apollo_index_table_create ( &itable, 2 * options->prealloc_index_count, &options->temp_allocator );
    }

    apollo_sb_prealloc ( f_pos, options->prealloc_vertex_count, options->temp_allocator );
    apollo_sb_prealloc ( f_tex, options->prealloc_vertex_count, options->temp_allocator );
    apollo_sb_prealloc ( f_norm, options->prealloc_vertex_count, options->temp_allocator );
    apollo_sb_prealloc ( m_idx, options->prealloc_index_count, options->temp_allocator );
    apollo_sb_prealloc ( m_vert, options->prealloc_vertex_count, options->temp_allocator );
    apollo_sb_prealloc ( meshes, options->prealloc_mesh_count, options->temp_allocator );
    apollo_sb_prealloc ( new_materials, options->prealloc_mesh_count, options->temp_allocator );
    apollo_sb_prealloc ( material_libs, options->prealloc_mesh_count, options->temp_allocator );
    // Start parsing
    ApolloResult result;
    int x;
    bool material_loaded = false;

    while ( ( x = fgetc ( file ) ) != EOF ) {
        switch ( x ) {
            case '\n':
                break;

            case '\b':
                break;

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
                    result = apollo_open_material_lib ( lib_name, apollo_sb_add ( material_libs, 1, options->temp_allocator ), *textures_bank, new_textures, &options->temp_allocator );

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
                if ( apollo_sb_count ( meshes ) == 0 ) {
                    ApolloOBJMesh* m = apollo_sb_add ( meshes, 1, options->temp_allocator );
                    apollo_initialize_mesh_obj ( m );
                    m->indices_offset = apollo_sb_count ( m_idx );
                }

                // Lookup the user materials
                uint32_t idx = apollo_find_material ( *materials_bank, mat_name );

                if ( idx != -1 ) {
                    apollo_sb_last ( meshes ).material = idx;
                    break;
                }

                // Lookup the additional materials that have been already loaded for this obj
                idx = apollo_find_material ( new_materials, mat_name );

                if ( idx != -1 ) {
                    apollo_sb_last ( meshes ).material = idx + apollo_sb_count ( *materials_bank );
                    break;
                }

                // Lookup the entire material libs that have been linked so far by this obj
                for ( size_t i = 0; i < apollo_sb_count ( material_libs ); ++i ) {
                    idx = apollo_find_material ( material_libs[i].materials, mat_name );

                    if ( idx != -1 ) {
                        apollo_sb_push ( new_materials, material_libs[i].materials[idx], options->temp_allocator );
                        apollo_sb_last ( meshes ).material = apollo_sb_count ( *materials_bank ) + apollo_sb_count ( new_materials ) - 1;
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
                        ApolloFloat3* pos = apollo_sb_add ( f_pos, 1, options->temp_allocator );

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
                        ApolloFloat2* uv = apollo_sb_add ( f_tex, 1, options->temp_allocator );

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
                        ApolloFloat3* norm = apollo_sb_add ( f_norm, 1, options->temp_allocator );

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

            // Treating groups and objects the same way
            case 'g':
            case 'o':
                // New mesh
                x = fgetc ( file );

                switch ( x ) {
                    case ' ': {
                        ApolloOBJMesh* m = apollo_sb_add ( meshes, 1, options->temp_allocator );
                        apollo_initialize_mesh_obj ( m );
                        m->indices_offset = apollo_sb_count ( m_idx, options->temp_allocator );
                        fscanf ( file, "%s", m->name );
                        break;
                    }

                    default:
                        while ( fgetc ( file ) != '\n' )
                            ;
                }

                break;

            case 's':
                x = fgetc ( file );
                x = fgetc ( file );

                if ( apollo_sb_count ( meshes ) == 0 ) {
                    ApolloOBJMesh* m = apollo_sb_add ( meshes, 1, options->temp_allocator );
                    apollo_initialize_mesh_obj ( m );
                    m->indices_offset = apollo_sb_count ( m_idx );
                }

                if ( x == '1' ) {
                    apollo_sb_last ( meshes ).smoothing_group = 1;
                } else {
                    apollo_sb_last ( meshes ).smoothing_group = 0;
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
                            apollo_sb_push ( m_idx, first_vertex_index, options->temp_allocator );
                            apollo_sb_push ( m_idx, last_vertex_index, options->temp_allocator );
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
                                                pos_index = data > 0 ? data - 1 : apollo_sb_count ( f_pos ) + data;
                                                break;

                                            case 1:
                                                tex_index = data > 0 ? data - 1 : apollo_sb_count ( f_tex ) + data;
                                                break;

                                            case 2:
                                                norm_index = data > 0 ? data - 1 : apollo_sb_count ( f_norm ) + data;
                                                break;
                                        }

                                        data_piece_len = 0;
                                    }

                                    ++data_piece_index;
                                }
                            } while ( vertex_str[i++] != '\0' );
                        }
                        // Preallocate index
                        uint32_t* index = apollo_sb_add ( m_idx, 1, options->temp_allocator );

                        // In an OBJ file, the faces mark the end of a mesh.
                        // However it could be that the first mesh was implicitly started (not listed in the file), so we consider that case,
                        if ( apollo_sb_count ( meshes ) == 0 ) {
                            ApolloOBJMesh* m = apollo_sb_add ( meshes, 1, options->temp_allocator );
                            apollo_initialize_mesh_obj ( m );
                            m->indices_offset = apollo_sb_count ( m_idx ) - 1;
                        }

                        // Finalize the vertex, testing for overlaps if the mesh is smooth and updating the first/last face vertex indices
                        bool duplicate = false;

                        if ( options->remove_vertex_duplicates &&  apollo_sb_last ( meshes ).smoothing_group == 1 ) {
                            ApolloVertexTableItem* lookup = apollo_vertex_table_lookup ( &vtable, &f_pos[pos_index] );

                            if ( lookup != NULL ) {
                                *index = lookup->value;
                                duplicate = true;
                            }
                        }

                        if ( !duplicate ) {
                            // If it's not a duplicate, add it as a new vertex
                            ApolloOBJVertex* vertex = apollo_sb_add ( m_vert, 1, options->temp_allocator );
                            vertex->pos = f_pos[pos_index];
                            vertex->tex = tex_index != -1 ? f_tex[tex_index] : ApolloFloat2{ 0, 0 };
                            vertex->norm = norm_index != -1 ? f_norm[norm_index] : ApolloFloat3{ 0, 0, 0 };
                            *index = apollo_sb_count ( m_vert ) - 1;

                            if ( options->remove_vertex_duplicates ) {
                                ApolloVertexTableItem item = { vertex->pos, *index };
                                apollo_vertex_table_insert ( &vtable, &item, &options->temp_allocator );
                            }
                        }

                        // Store the index into the adjacency table
                        if ( options->recompute_vertex_normals ) {
                            ApolloAdjacencyTableItem* adj_lookup = apollo_adjacency_table_lookup ( &atable, *index );

                            if ( adj_lookup == NULL ) {
                                adj_lookup = apollo_adjacency_table_insert ( &atable, *index, &options->temp_allocator );
                            }

                            apollo_adjacency_item_add_unique ( adj_lookup, face_count, &options->temp_allocator );
                        }

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
    fclose ( file );

    for ( size_t i = 0; i < apollo_sb_count ( material_libs ); ++i ) {
        fclose ( material_libs[i].file );
    }

    // Flip faces?
    if ( options->flip_faces_winding_order ) {
        for ( size_t i = 0; i < apollo_sb_count ( m_idx ); i += 3 ) {
            uint32_t idx = m_idx[i];
            m_idx[i] = m_idx[i + 2];
            m_idx[i + 2] = idx;
        }
    }

    // The unnnecessary scoping is just so that it is possible to call goto error.
    {
        ApolloFloat3* face_normals = NULL;
        ApolloFloat3* face_tangents = NULL;
        ApolloFloat3* face_bitangents = NULL;

        if ( options->compute_face_normals ) {
            apollo_sb_add ( face_normals, face_count, options->temp_allocator );
        }

        if ( options->compute_tangents ) {
            apollo_sb_add ( face_tangents, face_count, options->temp_allocator );
        }

        if ( options->compute_bitangents ) {
            apollo_sb_add ( face_bitangents, face_count, options->temp_allocator );
        }

        // Compute face normals
        if ( options->compute_face_normals ) {
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
        }

        // Recompute the vertex normals for smooth meshes
        if ( options->recompute_vertex_normals ) {
            for ( size_t m = 0; m < apollo_sb_count ( meshes ); ++m ) {
                size_t mesh_index_base = meshes[m].indices_offset;
                size_t mesh_index_count = m < apollo_sb_count ( meshes ) - 1 ? meshes[m + 1].indices_offset - mesh_index_base : apollo_sb_count ( m_idx ) - mesh_index_base;
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
                        apollo_index_table_insert ( &itable, m_idx[mesh_index_base + i], &options->temp_allocator );
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
        }

        // Compute tangents
        if ( options->compute_tangents ) {
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
            }
        }

        // Compute bitangents
        if ( options->compute_bitangents ) {
            for ( size_t i = 0; i < face_count; ++i ) {
                // x prod
                face_bitangents[i].x = face_normals[i].y * face_tangents[i].z - face_normals[i].z * face_tangents[i].y;
                face_bitangents[i].y = face_normals[i].z * face_tangents[i].x - face_normals[i].x * face_tangents[i].z;
                face_bitangents[i].z = face_normals[i].x * face_tangents[i].y - face_normals[i].y * face_tangents[i].x;
            }
        }

        // Build ApolloModel
#define APOLLO_FINAL_ALLOC(T, n) (T*) options->final_allocator.alloc(&options->final_allocator, sizeof(T) * n, 16);
        // Vertices
        model->vertex_data.pos_x =  APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.pos_y =  APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.pos_z =  APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.norm_x = APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.norm_y = APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.norm_z = APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.tex_u =  APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_data.tex_v =  APOLLO_ALLOC_ALIGN ( &options->final_allocator, float, apollo_sb_count ( m_vert ), 16 );
        model->vertex_count = apollo_sb_count ( m_vert );

        for ( size_t i = 0; i < apollo_sb_count ( m_vert ); ++i ) {
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
        model->meshes = APOLLO_FINAL_ALLOC ( ApolloMesh, apollo_sb_count ( meshes ) );
        model->mesh_count = apollo_sb_count ( meshes );

        for ( size_t i = 0; i < apollo_sb_count ( meshes ); ++i ) {
            strcpy ( model->meshes[i].name, meshes[i].name );
            memset ( &model->meshes[i].face_data, 0, sizeof ( ApolloMeshFaceData ) );
            // Faces
            int mesh_index_base = meshes[i].indices_offset;
            size_t mesh_index_count = i < apollo_sb_count ( meshes ) - 1 ? meshes[i + 1].indices_offset - mesh_index_base : apollo_sb_count ( m_idx ) - mesh_index_base;
            int mesh_face_count = mesh_index_count / 3;

            if ( mesh_index_count % 3 != 0 ) {
                APOLLO_LOG_ERR ( "Malformed mesh file @ %s", filename );
                result = APOLLO_INVALID_FORMAT;
                goto error;
            }

            model->meshes[i].face_data.idx_a = APOLLO_FINAL_ALLOC ( uint32_t, mesh_face_count );
            model->meshes[i].face_data.idx_b = APOLLO_FINAL_ALLOC ( uint32_t, mesh_face_count );
            model->meshes[i].face_data.idx_c = APOLLO_FINAL_ALLOC ( uint32_t, mesh_face_count );

            if ( options->compute_face_normals ) {
                model->meshes[i].face_data.normal_x = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
                model->meshes[i].face_data.normal_y = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
                model->meshes[i].face_data.normal_z = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
            }

            if ( options->compute_tangents ) {
                model->meshes[i].face_data.tangent_x = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
                model->meshes[i].face_data.tangent_y = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
                model->meshes[i].face_data.tangent_z = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
            }

            if ( options->compute_bitangents ) {
                model->meshes[i].face_data.bitangent_x = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
                model->meshes[i].face_data.bitangent_y = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
                model->meshes[i].face_data.bitangent_z = APOLLO_FINAL_ALLOC ( float, mesh_face_count );
            }

            model->meshes[i].face_count = mesh_face_count;

            for ( size_t j = 0; j < mesh_face_count; ++j ) {
                model->meshes[i].face_data.idx_a[j] = m_idx[mesh_index_base + j * 3 + 0];
                model->meshes[i].face_data.idx_b[j] = m_idx[mesh_index_base + j * 3 + 1];
                model->meshes[i].face_data.idx_c[j] = m_idx[mesh_index_base + j * 3 + 2];
            }

            if ( options->compute_face_normals ) {
                for ( size_t j = 0; j < mesh_face_count; ++j ) {
                    model->meshes[i].face_data.normal_x[j] = face_normals[mesh_index_base / 3 + j].x;
                    model->meshes[i].face_data.normal_y[j] = face_normals[mesh_index_base / 3 + j].y;
                    model->meshes[i].face_data.normal_z[j] = face_normals[mesh_index_base / 3 + j].z;
                }
            }

            if ( options->compute_tangents ) {
                for ( size_t j = 0; j < mesh_face_count; ++j ) {
                    model->meshes[i].face_data.tangent_x[j] = face_tangents[mesh_index_base / 3 + j].x;
                    model->meshes[i].face_data.tangent_y[j] = face_tangents[mesh_index_base / 3 + j].y;
                    model->meshes[i].face_data.tangent_z[j] = face_tangents[mesh_index_base / 3 + j].z;
                }
            }

            if ( options->compute_bitangents ) {
                for ( size_t j = 0; j < mesh_face_count; ++j ) {
                    model->meshes[i].face_data.bitangent_x[j] = face_bitangents[mesh_index_base / 3 + j].x;
                    model->meshes[i].face_data.bitangent_y[j] = face_bitangents[mesh_index_base / 3 + j].y;
                    model->meshes[i].face_data.bitangent_z[j] = face_bitangents[mesh_index_base / 3 + j].z;
                }
            }

            // Material
            model->meshes[i].material_id = meshes[i].material;

            // AABB
            if ( options->compute_bounds ) {
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
    }

    // AABB & Sphere
    if ( options->compute_bounds ) {
        ApolloFloat3 min = m_vert[0].pos;
        ApolloFloat3 max = m_vert[0].pos;

        for ( size_t j = 1; j < apollo_sb_count ( m_vert ); ++j ) {
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

        for ( size_t j = 1; j < apollo_sb_count ( m_vert ); ++j ) {
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

    // Update materials bank
    {
        ApolloMaterial* new_materials_base = apollo_sb_add ( *materials_bank, apollo_sb_count ( new_materials ), options->final_allocator );

        for ( size_t i = 0; i < apollo_sb_count ( new_materials ); ++i ) {
            new_materials_base[i] = new_materials[i];
        }
    }
    // Update textures bank
    {
        ApolloTexture* new_textures_base = apollo_sb_add ( *textures_bank, apollo_sb_count ( new_textures ), options->final_allocator );

        for ( size_t i = 0; i < apollo_sb_count ( new_textures ); ++i ) {
            new_textures_base[i] = new_textures[i];
        }
    }
    // Clean up and return
    apollo_filter_invalid_models ( model );
    apollo_vertex_table_destroy ( &vtable, &options->temp_allocator );
    apollo_adjacency_table_destroy ( &atable, &options->temp_allocator );
    apollo_index_table_destroy ( &itable, &options->temp_allocator );
    apollo_sb_free ( new_materials, options->temp_allocator );
    apollo_sb_free ( material_libs, options->temp_allocator );
    apollo_sb_free ( f_pos, options->temp_allocator );
    apollo_sb_free ( f_norm, options->temp_allocator );
    apollo_sb_free ( f_tex, options->temp_allocator );
    apollo_sb_free ( m_idx, options->temp_allocator );
    apollo_sb_free ( m_vert, options->temp_allocator );
    apollo_sb_free ( meshes, options->temp_allocator );
    return APOLLO_SUCCESS;
error:
    apollo_vertex_table_destroy ( &vtable, &options->temp_allocator );
    apollo_adjacency_table_destroy ( &atable, &options->temp_allocator );
    apollo_index_table_destroy ( &itable, &options->temp_allocator );
    apollo_sb_free ( new_materials, options->temp_allocator );
    apollo_sb_free ( material_libs, options->temp_allocator );
    apollo_sb_free ( f_pos, options->temp_allocator );
    apollo_sb_free ( f_norm, options->temp_allocator );
    apollo_sb_free ( f_tex, options->temp_allocator );
    apollo_sb_free ( m_idx, options->temp_allocator );
    apollo_sb_free ( m_vert, options->temp_allocator );
    apollo_sb_free ( meshes, options->temp_allocator );
    return result;
}

void apollo_dump_model_obj ( const ApolloModel* model, const ApolloMaterial* materials, const ApolloTexture* textures, const char* base_path ) {
    char obj_filename[256];
    char mat_filename[256];
    strcpy ( obj_filename, base_path );
    strcpy ( obj_filename + strlen ( obj_filename ), model->name );
    strcpy ( obj_filename + strlen ( obj_filename ), ".obj" );
    strcpy ( mat_filename, base_path );
    strcpy ( mat_filename + strlen ( mat_filename ), model->name );
    strcpy ( mat_filename + strlen ( mat_filename ), ".mtl" );
    FILE* obj_file = fopen ( obj_filename, "w" );
    fprintf ( obj_file, "#%s\n", model->name );
    fprintf ( obj_file, "#pos\n" );

    for ( size_t i = 0; i < model->vertex_count; ++i ) {
        fprintf ( obj_file, "v %f %f %f\n", model->vertex_data.pos_x[i], model->vertex_data.pos_y[i], model->vertex_data.pos_z[i] );
    }

    fprintf ( obj_file, "\n" );
    fprintf ( obj_file, "#tex\n" );

    for ( size_t i = 0; i < model->vertex_count; ++i ) {
        fprintf ( obj_file, "vt %f %f\n", model->vertex_data.tex_u[i], model->vertex_data.tex_v[i] );
    }

    fprintf ( obj_file, "\n" );
    fprintf ( obj_file, "#norm\n" );

    for ( size_t i = 0; i < model->vertex_count; ++i ) {
        fprintf ( obj_file, "vn %f %f %f\n", model->vertex_data.norm_x[i], model->vertex_data.norm_y[i], model->vertex_data.norm_z[i] );
    }

    fprintf ( obj_file, "\n" );

    for ( size_t i = 0; i < model->mesh_count; ++i ) {
        ApolloMesh* mesh = &model->meshes[i];
        ApolloMeshFaceData* data = &mesh->face_data;
        fprintf ( obj_file, "#o %s\n", mesh->name );

        for ( size_t j = 0; j < mesh->face_count; ++j ) {
            uint32_t a = data->idx_a[j] + 1;
            uint32_t b = data->idx_b[j] + 1;
            uint32_t c = data->idx_c[j] + 1;
            fprintf ( obj_file, "f %u/%u/%u %u/%u/%u %u/%u/%u\n", a, a, a, b, b, b, c, c, c );
        }

        fprintf ( obj_file, "\n" );
    }

    fclose ( obj_file );
    // Material
    /*
    typedef struct ApolloMaterial {
    float       ior;                    // Ni
    float       albedo[3];              // Kd
    uint32_t    albedo_texture;         // map_Kd
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
    */
    FILE* mat_file = fopen ( mat_filename, "w" );

    for ( size_t i = 0; i < model->mesh_count; ++i ) {
        ApolloMesh* mesh = &model->meshes[i];
        const ApolloMaterial* mat = &materials[mesh->material_id];
        fprintf ( mat_file, "newmtl %s\n", mat->name );
        const char*  illum[] = { "diffuse", "specular", "mirror", "pbr" };
        fprintf ( mat_file, "illum %s\n", illum[mat->bsdf] );
        fprintf ( mat_file, "Ni %f\n", mat->ior );

        if ( mat->albedo_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "map_Kd %s\n", textures[mat->albedo_texture].name );
        } else {
            fprintf ( mat_file, "Kd %f %f %f\n", mat->albedo[0], mat->albedo[1], mat->albedo[2] );
        }

        if ( mat->specular_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "map_Ks %s\n", textures[mat->specular_texture].name );
        } else {
            fprintf ( mat_file, "Ks %f %f %f\n", mat->specular[0], mat->specular[1], mat->specular[2] );
        }

        if ( mat->specular_exp_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "map_Ns %s\n", textures[mat->specular_exp_texture].name );
        } else {
            fprintf ( mat_file, "Ns %f\n", mat->specular_exp );
        }

        if ( mat->bump_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "bump %s\n", textures[mat->bump_texture].name );
        }

        if ( mat->displacement_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "disp %s\n", textures[mat->displacement_texture].name );
        }

        if ( mat->roughness_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "map_Pr %s\n", textures[mat->roughness_texture].name );
        } else {
            fprintf ( mat_file, "Pr %f\n", mat->roughness );
        }

        if ( mat->metallic_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "map_Pm %s\n", textures[mat->metallic_texture].name );
        } else {
            fprintf ( mat_file, "Pm %f\n", mat->metallic );
        }

        if ( mat->emissive_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "map_Ke %s\n", textures[mat->emissive_texture].name );
        } else {
            fprintf ( mat_file, "Ke %f %f %f\n", mat->emissive[0], mat->emissive[1], mat->emissive[2] );
        }

        if ( mat->normal_texture != APOLLO_TEXTURE_NONE ) {
            fprintf ( mat_file, "norm %s\n", textures[mat->normal_texture].name );
        }

        fprintf ( mat_file, "\n" );
    }
}

//--------------------------------------------------------------------------------------------------
// IndexTable
//--------------------------------------------------------------------------------------------------
void apollo_index_table_create ( ApolloIndexTable* table, uint32_t capacity, ApolloAllocator* allocator ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 ); // is power of 2
    table->items = APOLLO_ALLOC ( allocator, uint32_t, capacity );
    memset ( table->items, 0, sizeof ( uint32_t ) * capacity );
    table->capacity = capacity;
    table->count = 0;
    table->access_mask = capacity - 1;
    table->is_zero_id_slot_used = false;
}

void apollo_index_table_destroy ( ApolloIndexTable* table, ApolloAllocator* allocator ) {
    APOLLO_FREE ( allocator, table->items, table->capacity );
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

void apollo_index_table_resize ( ApolloIndexTable* table, uint32_t new_capacity, ApolloAllocator* allocator ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 ); // is power of 2
    uint32_t* old_items = table->items;
    uint32_t old_capacity = table->capacity;
    table->items = APOLLO_ALLOC ( allocator, uint32_t, new_capacity );
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

    APOLLO_FREE ( allocator, old_items, old_capacity );
}

void apollo_index_table_insert ( ApolloIndexTable* table, uint32_t item, ApolloAllocator* allocator ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_index_table_resize ( table, table->capacity * 2, allocator );
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
void apollo_vertex_table_create ( ApolloVertexTable* table, uint32_t capacity, ApolloAllocator* allocator ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 ); // is power of 2
    table->items = APOLLO_ALLOC ( allocator, ApolloVertexTableItem, capacity );
    memset ( table->items, 0, sizeof ( *table->items ) * capacity );
    table->capacity = capacity;
    table->count = 0;
    table->access_mask = capacity - 1;
    table->is_zero_key_item_used = false;
    table->zero_key_item = { { 0, 0, 0 }, 0 };
}

void apollo_vertex_table_destroy ( ApolloVertexTable* table, ApolloAllocator* allocator ) {
    APOLLO_FREE ( allocator, table->items, table->capacity );
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

void apollo_vertex_table_resize ( ApolloVertexTable* table, uint32_t new_capacity, ApolloAllocator* allocator ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 ); // is power of 2
    ApolloVertexTableItem* old_items = table->items;
    uint32_t old_capacity = table->capacity;
    table->items = APOLLO_ALLOC ( allocator, ApolloVertexTableItem, new_capacity );
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

    APOLLO_FREE ( allocator, old_items, old_capacity );
}

void apollo_vertex_table_insert ( ApolloVertexTable* table, const ApolloVertexTableItem* item, ApolloAllocator* allocator ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_vertex_table_resize ( table, table->capacity * 2, allocator );
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
bool apollo_adjacency_item_add_unique ( ApolloAdjacencyTableItem* item, uint32_t value, ApolloAllocator* allocator ) {
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
            item->next = ( ApolloAdjacencyTableExtension* ) APOLLO_ALLOC ( allocator, ApolloAdjacencyTableExtension, 1, 64 );
            memset ( item->next, 0, sizeof ( *item->next ) );
            ext = item->next;
        }
    }

    if ( ext->count == APOLLO_ADJACENCY_EXTENSION_CAPACITY ) {
        ext->next = ( ApolloAdjacencyTableExtension* ) APOLLO_ALLOC ( allocator, ApolloAdjacencyTableExtension, 1, 64 );
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

void apollo_adjacency_table_create ( ApolloAdjacencyTable* table, uint32_t capacity, ApolloAllocator* allocator ) {
    assert ( ( capacity & ( capacity - 1 ) ) == 0 );
    table->count = 0;
    table->capacity = capacity;
    table->access_mask = capacity - 1;
    table->items = APOLLO_ALLOC ( allocator, ApolloAdjacencyTableItem, capacity );
    memset ( table->items, 0, sizeof ( ApolloAdjacencyTableItem ) * capacity );
    memset ( &table->zero_key_item, 0, sizeof ( table->zero_key_item ) );
}

void apollo_adjacency_table_destroy ( ApolloAdjacencyTable* table, ApolloAllocator* allocator ) {
    for ( size_t i = 0; i < table->capacity; ++i ) {
        ApolloAdjacencyTableExtension* ext = table->items[i].next;

        while ( ext ) {
            ApolloAdjacencyTableExtension* p = ext;
            ext = ext->next;
            APOLLO_FREE ( allocator, p, 1 );
        }
    }

    APOLLO_FREE ( allocator, table->items, table->capacity );
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

void apollo_adjacency_table_resize ( ApolloAdjacencyTable* table, uint32_t new_capacity, ApolloAllocator* allocator ) {
    assert ( ( new_capacity & ( new_capacity - 1 ) ) == 0 );
    ApolloAdjacencyTableItem* old_items = table->items;
    uint32_t old_capacity = table->capacity;
    table->items = APOLLO_ALLOC ( allocator, ApolloAdjacencyTableItem, new_capacity );
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

    APOLLO_FREE ( allocator, old_items, old_capacity );
}

ApolloAdjacencyTableItem* apollo_adjacency_table_insert ( ApolloAdjacencyTable* table, uint32_t key, ApolloAllocator* allocator ) {
    if ( ( table->count + 1 ) * 4 >= table->capacity * 3 ) {
        apollo_adjacency_table_resize ( table, table->capacity * 2, allocator );
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
static void* apollo__sbgrowf ( void* arr, int increment, int itemsize, ApolloAllocator* allocator ) {
    int dbl_cur = arr ? 2 * apollo__sbm ( arr ) : 0;
    int min_needed = apollo_sb_count ( arr ) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int* p = arr ? ( int* ) allocator->realloc ( allocator->p, apollo__sbraw ( arr ), apollo__sbm ( arr ) * itemsize, itemsize * m + sizeof ( int ) * 2, itemsize >= 4 ? itemsize : 4 )
             : ( int* ) allocator->alloc ( allocator->p, itemsize * m + sizeof ( int ) * 2, itemsize >= 4 ? itemsize : 4 );

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