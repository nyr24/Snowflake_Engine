#pragma once

#include "sf_core/defines.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanDevice;

struct VulkanDescriptorSetLayout {
public:
    VkDescriptorSetLayout handle;
public:
    static void create(const VulkanDevice& device, u32 binding_count, VkDescriptorSetLayoutBinding* bindings, VulkanDescriptorSetLayout& out_layout);  
    static void create_binding(u32 index, VkDescriptorType type, u32 count, VkShaderStageFlags stage_flags, VkDescriptorSetLayoutBinding& out_binding);
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

} // sf
