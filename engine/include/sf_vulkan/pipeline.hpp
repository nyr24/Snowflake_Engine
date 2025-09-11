#pragma once

#include "sf_containers/fixed_array.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/swapchain.hpp"
#include <span>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace sf {

struct VulkanContext;
struct VulkanCommandBuffer;

struct VulkanDescriptorSetLayout {
public:
    VkDescriptorSetLayout handle;
public:
    static void create(const VulkanDevice& device, std::span<VkDescriptorSetLayoutBinding> bindings, VulkanDescriptorSetLayout& out_layout);  
    static void create_bindings(u32 index, VkDescriptorType descriptor_type, u32 descriptor_count, VkShaderStageFlags stage_flags, std::span<VkDescriptorSetLayoutBinding> out_bindings);
    void destroy(const VulkanDevice& device);
};

struct VulkanDescriptorPool {
public:
    VkDescriptorPool handle;
public:
    static void create(const VulkanDevice& device, u32 descriptor_count, VkDescriptorType type, u32 max_sets, VulkanDescriptorPool& out_pool);
    void allocate_sets(const VulkanDevice& device, u32 count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets);
    void destroy(const VulkanDevice& device);
};

// THINK: do we need to tie vertex & index buffers to pipeline also?
struct VulkanShaderPipeline {
public:
    static constexpr u32 MAX_ATTRIB_COUNT{ 3 };
    
    VulkanGlobalUniformBufferObject                                               global_ubo;
    FixedArray<VulkanPushConstantBlock, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>    push_constant_block;
    FixedArray<VkVertexInputAttributeDescription, MAX_ATTRIB_COUNT>               attrib_description;
    FixedArray<VulkanDescriptorSetLayout, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>  descriptor_layouts;
    FixedArray<VkDescriptorSet, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>            descriptor_sets;
    VulkanDescriptorPool         descriptor_pool;
    VkShaderModule               shader_handle;
    VkPipeline                   pipeline_handle;
    VkPipelineLayout             pipeline_layout;
public:
    VulkanShaderPipeline();
    
    static bool create(
        const VulkanContext& context,
        const char* shader_file_name,
        VkPrimitiveTopology primitive_topology,
        bool enable_color_blend,
        std::span<VkVertexInputAttributeDescription> attrib_descriptions,
        VulkanShaderPipeline& out_pipeline
    );
    void destroy(const VulkanDevice& device);
    void bind(const VulkanCommandBuffer& cmd_buffer, u32 curr_frame);
private:
    static void create_descriptor_pool(const VulkanDevice& device, u32 descriptor_count, VkDescriptorType type, u32 max_sets, VkDescriptorPool& out_pool);
    void init_descriptor_layouts_and_sets(const VulkanDevice& device, std::span<VkDescriptorSetLayoutBinding> bindings);
};

} // sf
