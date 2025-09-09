#include "sf_vulkan/descriptor.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

void VulkanDescriptorPool::create(const VulkanDevice& device, u32 descriptor_count, VkDescriptorType type, u32 max_sets, VulkanDescriptorPool& out_pool) {
    VkDescriptorPoolSize pool_size{
        .type = type,
        .descriptorCount = descriptor_count
    };

    VkDescriptorPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = max_sets,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateDescriptorPool(device.logical_device, &create_info, nullptr, &out_pool.handle)); 
}

void VulkanDescriptorPool::allocate_sets(const VulkanDevice& device, u32 count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets) {
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = handle,
        .descriptorSetCount = count,
        .pSetLayouts = layouts
    };

    sf_vk_check(vkAllocateDescriptorSets(device.logical_device, &alloc_info, sets));
}

void VulkanDescriptorPool::destroy(const VulkanDevice& device) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyDescriptorPool(device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

void VulkanDescriptorSetLayout::create(const VulkanDevice& device, u32 binding_count, VkDescriptorSetLayoutBinding* bindings,VulkanDescriptorSetLayout& out_layout) {
    VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = binding_count,
        .pBindings = bindings,
    };
    // TODO: custom allocator
    sf_vk_check(vkCreateDescriptorSetLayout(device.logical_device, &descriptor_layout_create_info, nullptr, &out_layout.handle));
}

void VulkanDescriptorSetLayout::create_binding(u32 index, VkDescriptorType type, u32 count, VkShaderStageFlags stage_flags, VkDescriptorSetLayoutBinding& out_binding) {
    out_binding.binding = index,
    out_binding.descriptorType = type,
    out_binding.descriptorCount = count,
    out_binding.stageFlags = stage_flags,
    out_binding.pImmutableSamplers = nullptr;
}

void VulkanDescriptorSetLayout::destroy(const VulkanDevice& device) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyDescriptorSetLayout(device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

} // sf
