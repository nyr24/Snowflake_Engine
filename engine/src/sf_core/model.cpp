#include "sf_core/model.hpp"
#include "assimp/material.h"
#include "assimp/mesh.h"
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/mesh.hpp"
#include "sf_vulkan/shared_types.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string_view>

namespace sf {

Assimp::Importer importer;

static void process_ai_node(aiNode* node, const aiScene* scene, Model& model, StackAllocator& alloc, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer);
static void process_ai_mesh(aiMesh* ai_mesh, const aiScene* scene, StackAllocator& alloc, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, Mesh& result_mesh);
static Material& acquire_mesh_material(
    aiMaterial* ai_mat,
    FixedArray<TextureInputConfig, Material::MAX_DIFFUSE_COUNT>&   diffuse_tex_configs,
    FixedArray<TextureInputConfig, Material::MAX_SPECULAR_COUNT>&  specular_tex_configs,
    FixedArray<TextureInputConfig, Material::MAX_AMBIENT_COUNT>&   ambient_tex_configs,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    StackAllocator& alloc
);

void Model::create(ArenaAllocator& alloc, Model& out_model) {
    out_model.meshes.set_allocator(&alloc);   
}

bool Model::load(std::string_view file_name, ArenaAllocator& main_alloc, StackAllocator& temp_alloc, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, Model& out_model) {
    Model::create(main_alloc, out_model);
    
#ifdef SF_DEBUG
    std::string_view init_path = "build/debug/engine/assets/models/";
#else
    std::string_view init_path = "build/release/engine/assets/models/";
#endif
    StringBacked<StackAllocator> model_path(init_path.size() + file_name.size() + 1, &temp_alloc);
    model_path.append_sv(init_path);
    model_path.append_sv(file_name);
    model_path.append('\0');

    const aiScene* scene = importer.ReadFile(model_path.data(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        LOG_ERROR("Failed to load model with name: {}\nError Message: {}", file_name, importer.GetErrorString());
        return false;
    }

    out_model.meshes.reserve(scene->mNumMeshes);
    process_ai_node(scene->mRootNode, scene, out_model, temp_alloc, device, cmd_buffer); 

    return true;
}

static void process_ai_node(aiNode* node, const aiScene* scene, Model& model, StackAllocator& alloc, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer) {
    for (u32 i{0}; i < node->mNumMeshes; ++i) {
        aiMesh* ai_mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh mesh;
        process_ai_mesh(ai_mesh, scene, alloc, device, cmd_buffer, mesh);
        model.meshes.append(mesh);
    }

    for (u32 i{0}; i < node->mNumChildren; ++i) {
        process_ai_node(node->mChildren[i], scene, model, alloc, device, cmd_buffer);
    }
}

// https://learnopengl.com/code_viewer_gh.php?code=includes/learnopengl/model.h
static void process_ai_mesh(aiMesh* ai_mesh, const aiScene* scene, StackAllocator& alloc, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, Mesh& out_mesh) {
    SF_ASSERT_MSG(ai_mesh, "Should be valid pointer");

    DynamicArrayBacked<Vertex, StackAllocator>   vertices(ai_mesh->mNumVertices, &alloc);
    DynamicArrayBacked<u32, StackAllocator>      indices(ai_mesh->mNumFaces, &alloc);

    // DynamicArrayBacked<TextureInputConfig, StackAllocator> tex_configs(texture_count, &alloc);
    
    const bool has_normals = ai_mesh->HasNormals();
    const bool has_tex_coords = ai_mesh->mTextureCoords[0] != nullptr;

    for (u32 i{0}; i < ai_mesh->mNumVertices; ++i) {
        Vertex vert;
        aiVector3f ai_vert = ai_mesh->mVertices[i];
        
        vert.pos.x = ai_vert.x;
        vert.pos.y = ai_vert.y;
        vert.pos.z = ai_vert.z;

        if (has_normals) {
            aiVector3f ai_normal = ai_mesh->mNormals[i];
            vert.normal.x = ai_normal.x;
            vert.normal.y = ai_normal.y;
            vert.normal.z = ai_normal.z;
        }
        if (has_tex_coords) {
            aiVector3f ai_tex_coord = ai_mesh->mTextureCoords[0][i];
            vert.texture_coord.x = ai_tex_coord.x;
            vert.texture_coord.y = ai_tex_coord.y;
        }

        vertices.append(vert);
    }

    for(u32 i{0}; i < ai_mesh->mNumFaces; i++) {
        aiFace face = ai_mesh->mFaces[i];
        for(u32 j{0}; j < face.mNumIndices; j++) {
            indices.append(face.mIndices[j]);        
        }
    }

    Geometry& geometry = GeometrySystem::create_geometry_and_get(std::move(vertices), std::move(indices));
    out_mesh.set_geometry(&geometry);

    FixedArray<TextureInputConfig, Material::MAX_DIFFUSE_COUNT>   diffuse_tex_configs;
    FixedArray<TextureInputConfig, Material::MAX_SPECULAR_COUNT>  specular_tex_configs;
    FixedArray<TextureInputConfig, Material::MAX_AMBIENT_COUNT>   ambient_tex_configs;
    
    // NOTE: if we would require more texture configs we should split calls to collect_${type}_textures
    // NOTE: not a stack memory reference (safe)
    Material& mesh_material = acquire_mesh_material(scene->mMaterials[ai_mesh->mMaterialIndex], diffuse_tex_configs, specular_tex_configs, ambient_tex_configs, device, cmd_buffer, alloc);
    out_mesh.set_material(&mesh_material);
}

static Material& acquire_mesh_material(
    aiMaterial* ai_mat,
    FixedArray<TextureInputConfig, Material::MAX_DIFFUSE_COUNT>&   diffuse_tex_configs,
    FixedArray<TextureInputConfig, Material::MAX_SPECULAR_COUNT>&  specular_tex_configs,
    FixedArray<TextureInputConfig, Material::MAX_AMBIENT_COUNT>&   ambient_tex_configs,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    StackAllocator& alloc
) {
#ifdef SF_DEBUG
    const u32 diffuse_tex_count = ai_mat->GetTextureCount(aiTextureType_DIFFUSE);
    const u32 specular_tex_count = ai_mat->GetTextureCount(aiTextureType_SPECULAR);
    const u32 ambient_tex_count = ai_mat->GetTextureCount(aiTextureType_AMBIENT);

    LOG_INFO("material {} has tex counts: diff - {}, spec - {}, amb - {}", ai_mat->GetName().C_Str(), diffuse_tex_count, specular_tex_count, ambient_tex_count);
#endif
    
    // collect all types of texture configs
    const u32 min_diffuse_count = std::min(ai_mat->GetTextureCount(aiTextureType_DIFFUSE), diffuse_tex_configs.capacity());

    for(u32 i{0}; i < min_diffuse_count; i++) {
        aiString tex_path;
        ai_mat->GetTexture(aiTextureType_DIFFUSE, i, &tex_path);
        diffuse_tex_configs.append(TextureInputConfig{{ tex_path.data, tex_path.length }});
    }

    const u32 min_specular_count = std::min(ai_mat->GetTextureCount(aiTextureType_SPECULAR), specular_tex_configs.capacity());

    for(u32 i{0}; i < min_specular_count; i++) {
        aiString tex_path;
        ai_mat->GetTexture(aiTextureType_SPECULAR, i, &tex_path);
        specular_tex_configs.append(TextureInputConfig{{ tex_path.data, tex_path.length }});
    }

    const u32 min_ambient_count = std::min(ai_mat->GetTextureCount(aiTextureType_AMBIENT), ambient_tex_configs.capacity());

    for(u32 i{0}; i < min_ambient_count; i++) {
        aiString tex_path;
        ai_mat->GetTexture(aiTextureType_AMBIENT, i, &tex_path);
        ambient_tex_configs.append(TextureInputConfig{{ tex_path.data, tex_path.length }});
    }

    // acquire a material
    return MaterialSystem::create_material_from_textures(device, cmd_buffer, alloc, diffuse_tex_configs.to_span(), specular_tex_configs.to_span(), ambient_tex_configs.to_span());
}

} // sf
