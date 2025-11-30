#pragma once

#include "sf_vulkan/shared_types.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "glm/fwd.hpp"
#include <span>
#include <string_view>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace sf {

Result<VkShaderModule> create_shader_module(const VulkanDevice& device, String<StackAllocator>&& shader_file_path);

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

struct MaterialUpdateData {
    Material*                material;
    u32                      descriptor_state_index;
};

// TODO: shader object with multiple pipelines (default/wireframe)
// struct VulkanPipelineConfig {};
// struct VulkanPipeline {};

struct EventContext;

struct VulkanShaderPipeline {
public:
    static constexpr u32 DIFFUSE_TEX_COUNT { 1 };
    static constexpr u32 SPECULAR_TEX_COUNT{ 1 };
    static constexpr u32 NORMAL_TEX_COUNT  { 1 };
    static constexpr u32 AMBIENT_TEX_COUNT { 1 };
    static constexpr u32 TEXTURE_COUNT { DIFFUSE_TEX_COUNT + SPECULAR_TEX_COUNT + AMBIENT_TEX_COUNT + NORMAL_TEX_COUNT };
    static constexpr u32 DESCRIPTOR_BINDING_COUNT{ 1 + TEXTURE_COUNT };

    static constexpr u32 MAX_OBJECT_COUNT{ 1024 };
    static constexpr u32 MAX_ATTRIB_COUNT{ 3 };
    static constexpr u32 MAX_DEFAULT_TEXTURES{ 20 };

    static constexpr FixedArray<TextureType, TEXTURE_COUNT> TEXTURE_TYPES{ TextureType::DIFFUSE, TextureType::SPECULAR, TextureType::NORMALS, TextureType::AMBIENT };

    struct VulkanDescriptorState {
        FixedArray<u32, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> generations;
    };

    struct ObjectShaderState {
        FixedArray<VkDescriptorSet, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>    descriptor_sets;
        FixedArray<VulkanDescriptorState, DESCRIPTOR_BINDING_COUNT>           descriptor_binding_states;
    };
public:
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
    u32                          descriptor_state_index_counter;
    // NOTE: TEMP
    u32                          default_texture_index;
public:
    VulkanShaderPipeline();
    
    static bool create(
        VulkanContext&                 context,
        std::string_view               shader_file_name,
        VkPrimitiveTopology            primitive_topology,
        bool                           enable_color_blend,
        const FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>& attrib_descriptions,
        VkViewport                     viewport,
        VkRect2D                       scissors,
        std::span<Texture*>            default_textures,
        VulkanShaderPipeline&          out_pipeline,
        StackAllocator&                alloc
    );
    void destroy(const VulkanDevice& device);
    void bind(const VulkanCommandBuffer& cmd_buffer, u32 curr_frame);
    void bind_object_descriptor_sets(VulkanCommandBuffer& cmd_buffer, u32 object_id, u32 curr_frame);
    void update_model(VulkanCommandBuffer& cmd_buffer, glm::mat4& model);
    void update_material(VulkanContext& context, VulkanCommandBuffer& cmd_buffer, MaterialUpdateData& render_data);
    u32  acquire_resouces(const VulkanDevice& device);
    void release_resouces(const VulkanDevice& device, u32 descriptor_state_index);
    static bool handle_swap_default_texture(u8 code, void* sender, void* listener_inst, Option<EventContext> context);
private:
    void create_attribute_descriptions(const FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>& config);
    void create_local_descriptors(const VulkanDevice& device);
    void update_local_descriptors(const VulkanDevice& device);
};

} // sf
