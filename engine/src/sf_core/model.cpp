#include "assimp/material.h"
#include "sf_vulkan/shared_types.hpp"
#include "sf_core/model.hpp"
#include <assimp/mesh.h>
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/io.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/mesh.hpp"
#include "sf_vulkan/pipeline.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string_view>

namespace sf {

Assimp::Importer importer;

static void process_ai_node(
    aiNode* node,
    const aiScene* scene,
    std::string_view texture_base_path,
    Model& model,
    StackAllocator& alloc,
    const VulkanDevice& device,
    VulkanShaderPipeline& shader,
    VulkanCommandBuffer& cmd_buffer
);

static void process_ai_mesh(
    aiMesh* ai_mesh,
    const aiScene* scene,
    std::string_view texture_base_path,
    StackAllocator& alloc,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    VulkanShaderPipeline& shader,
    Mesh& out_mesh
);

static Material& acquire_mesh_material(
    aiMaterial* ai_mat,
    std::string_view texture_base_path,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    StackAllocator& alloc
);

void Model::create(ArenaAllocator& alloc, Model& out_model) {
    out_model.meshes.set_allocator(&alloc);   
}

bool Model::load(
    std::string_view model_file_name,
    ArenaAllocator& main_alloc,
    StackAllocator& temp_alloc,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    VulkanShaderPipeline& shader,
    Model& out_model
){
    Model::create(main_alloc, out_model);
    std::string_view model_name = strip_extension_from_file_name(model_file_name);
    std::string_view model_ext = extract_extension_from_file_name(model_file_name);
    u32 model_name_cnt = model_name.size();
    
#ifdef SF_DEBUG
    std::string_view init_path = "build/debug/engine/assets/models/";
#else
    std::string_view init_path = "build/release/engine/assets/models/";
#endif
    String<StackAllocator> model_path(init_path.size() + model_name_cnt + 1 + model_name_cnt + 1 + model_ext.size() + 1, &temp_alloc);
    model_path.append_sv(init_path);
    model_path.append_sv(model_name);
    model_path.append('/');
    model_path.append_sv(model_name);
    model_path.append('.');
    model_path.append_sv(model_ext);
    model_path.ensure_null_terminated();

    std::string_view texture_base_path = strip_file_name_from_path(model_path.to_sv());

    const aiScene* scene = importer.ReadFile(model_path.data(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        LOG_ERROR("Failed to load model with name: {}\nError Message: {}", model_name, importer.GetErrorString());
        return false;
    }

    out_model.meshes.reserve(scene->mNumMeshes);
    process_ai_node(scene->mRootNode, scene, texture_base_path, out_model, temp_alloc, device, shader, cmd_buffer); 

    return true;
}

static void process_ai_node(
    aiNode* node,
    const aiScene* scene,
    std::string_view texture_base_path,
    Model& model,
    StackAllocator& alloc,
    const VulkanDevice& device,
    VulkanShaderPipeline& shader,
    VulkanCommandBuffer& cmd_buffer
) {
    for (u32 i{0}; i < node->mNumMeshes; ++i) {
        aiMesh* ai_mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh mesh;
        process_ai_mesh(ai_mesh, scene, texture_base_path, alloc, device, cmd_buffer, shader, mesh);
        model.meshes.append(mesh);
    }

    for (u32 i{0}; i < node->mNumChildren; ++i) {
        process_ai_node(node->mChildren[i], scene, texture_base_path, model, alloc, device, shader, cmd_buffer);
    }
}

static void process_ai_mesh(
    aiMesh* ai_mesh,
    const aiScene* scene,
    std::string_view texture_base_path,
    StackAllocator& alloc,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    VulkanShaderPipeline& shader,
    Mesh& out_mesh
)
{
    SF_ASSERT_MSG(ai_mesh, "Should be valid pointer");

    DynamicArray<Vertex, StackAllocator>   vertices(ai_mesh->mNumVertices, &alloc);
    DynamicArray<u32, StackAllocator>      indices(ai_mesh->mNumFaces, &alloc);

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
            // NOTE: TEMP flip y
            vert.texture_coord.y = 1.0f - ai_tex_coord.y;
        } else {
            vert.texture_coord.x = 0.0f;
            vert.texture_coord.y = 0.0f;
        }

        vertices.append(vert);
    }

    for(u32 i{0}; i < ai_mesh->mNumFaces; ++i) {
        aiFace face = ai_mesh->mFaces[i];
        for(u32 j{0}; j < face.mNumIndices; ++j) {
            indices.append(face.mIndices[j]);        
        }
    }

    GeometryView& geometry_view = GeometrySystem::create_geometry_and_get_view(std::move(vertices), std::move(indices));
    out_mesh.set_geometry_view(&geometry_view);

    // NOTE: not a stack memory reference (safe)
    Material& mesh_material = acquire_mesh_material(scene->mMaterials[ai_mesh->mMaterialIndex], texture_base_path, device, cmd_buffer, alloc);
    out_mesh.set_material(&mesh_material);

    out_mesh.descriptor_state_index = shader.acquire_resouces(device);
}

static Material& acquire_mesh_material(
    aiMaterial* ai_mat,
    std::string_view texture_base_path,
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    StackAllocator& alloc
) {
    FixedArray<TextureInputConfig, VulkanShaderPipeline::TEXTURE_COUNT> tex_configs;
    constexpr u32 CNT = VulkanShaderPipeline::TEXTURE_COUNT;

    for (u32 i{0}; i < CNT ; ++i) {
        const aiTextureType tex_type = static_cast<aiTextureType>(VulkanShaderPipeline::TEXTURE_TYPES[i]);
        const u32 texture_count_of_type = ai_mat->GetTextureCount(tex_type);
        if (texture_count_of_type == 0) {
            continue;
        }

        aiString tex_file_name;
        ai_mat->GetTexture(tex_type, i, &tex_file_name);
        if (tex_file_name.length == 0) {
            continue;
        }
        
        String<StackAllocator> tex_path(texture_base_path.size() + tex_file_name.length + 1, &alloc);
        tex_path.append_sv(texture_base_path);
        tex_path.append_sv(std::string_view(tex_file_name.C_Str(), tex_file_name.length));
        tex_path.ensure_null_terminated();
    
        tex_configs.append(TextureInputConfig{ std::move(tex_path), static_cast<TextureType>(tex_type) });
    }

    return MaterialSystem::create_material_from_textures(device, cmd_buffer, alloc, tex_configs.to_span());
}

} // sf
