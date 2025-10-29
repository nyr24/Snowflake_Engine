#pragma once

#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_containers/result.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_vulkan/texture.hpp"
#include "sf_containers/result.hpp"
#include <filesystem>
#include <span>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace fs = std::filesystem;

namespace sf {

struct VulkanContext;
struct VulkanCommandBuffer;

Result<VkShaderModule> create_shader_module(const VulkanDevice& device, fs::path&& shader_file_path);

struct VulkanDescriptorSetLayout {
public:
    VkDescriptorSetLayout handle;
public:
    static void create(const VulkanDevice& device, std::span<VkDescriptorSetLayoutBinding> bindings, VulkanDescriptorSetLayout& out_layout);  
    static void create_bindings(VkShaderStageFlags stage_flags, std::span<VkDescriptorType> descriptor_types, std::span<VkDescriptorSetLayoutBinding> out_bindings);
    void destroy(const VulkanDevice& device);
};

struct VulkanDescriptorPool {
public:
    VkDescriptorPool handle;
public:
    static void create(const VulkanDevice& device, std::span<VkDescriptorPoolSize> pool_sizes, u32 max_sets, VulkanDescriptorPool& out_pool);
    void allocate_sets(const VulkanDevice& device, u32 count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets) const;
    void destroy(const VulkanDevice& device);
};

constexpr u32 MAX_OBJECT_COUNT{ 1024 };
constexpr u32 OBJECT_SHADER_DESCRIPTOR_COUNT{ 2 };

struct VulkanDescriptorState {
    FixedArray<u32, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> generations;
};

// state of the object inside a shader
struct ObjectShaderState {
    FixedArray<VkDescriptorSet, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>           descriptor_sets;
    FixedArray<VulkanDescriptorState, OBJECT_SHADER_DESCRIPTOR_COUNT>            descriptor_states;
};

struct GeometryRenderData {
    glm::mat4               model;
    FixedArray<Texture*, 16> textures;
    u32                     id;

    GeometryRenderData()
    {
        textures.resize_to_capacity();
    }
};

// TODO: shader object with multiple pipelines (default/wireframe)
// struct VulkanPipelineConfig {};
// struct VulkanPipeline {};

struct EventContext;

struct VulkanShaderPipeline {
public:
    static constexpr u32 MAX_ATTRIB_COUNT{ 3 };
    static constexpr u32 MAX_DEFAULT_TEXTURES{ 10 };

    VulkanLocalUniformBufferObject                                                local_ubo;
    VulkanContext*                                                                context;
    VulkanDescriptorSetLayout                                                     object_descriptor_layout;
    VulkanDescriptorPool                                                          object_descriptor_pool;
    FixedArray<Texture*, MAX_DEFAULT_TEXTURES>                                    default_textures;
    FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>               attrib_descriptions;
    FixedArray<ObjectShaderState, MAX_OBJECT_COUNT>                               object_shader_states;
    VkShaderModule               shader_handle;
    VkPipeline                   pipeline_handle;
    VkPipelineLayout             pipeline_layout;
    VkViewport                   viewport;
    VkRect2D                     scissors;
    u32                          object_id_counter;
    // NOTE: TEMP
    u32                          default_texture_index;
public:
    VulkanShaderPipeline();
    
    static bool create(
        VulkanContext&                 context,
        const char*                    shader_file_name,
        VkPrimitiveTopology            primitive_topology,
        bool                           enable_color_blend,
        const FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>& attrib_descriptions,
        VkViewport                     viewport,
        VkRect2D                       scissors,
        std::span<const TextureInputConfig> default_texture_configs,
        VulkanShaderPipeline&          out_pipeline
    );
    void destroy(const VulkanDevice& device);
    void bind(const VulkanCommandBuffer& cmd_buffer, u32 curr_frame);
    void bind_object_descriptor_sets(VulkanCommandBuffer& cmd_buffer, u32 object_id, u32 curr_frame);
    void update_object_state(VulkanContext& context, GeometryRenderData& render_data);
    Result<u32> acquire_resouces(const VulkanDevice& device);
    void release_resouces(const VulkanDevice& device, u32 object_id);
    static bool handle_swap_default_texture(u8 code, void* sender, void* listener_inst, Option<EventContext> context);
private:
    void create_attribute_descriptions(const FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>& config);
    void create_local_descriptors(const VulkanDevice& device);
    void update_local_descriptors(const VulkanDevice& device);
    void create_default_textures(std::span<const TextureInputConfig> configs);
};

} // sf
