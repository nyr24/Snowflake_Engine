#pragma once

#include "sf_containers/fixed_array.hpp"
#include "sf_vulkan/descriptor.hpp"
#include "sf_vulkan/device.hpp"
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace sf {

struct VulkanContext;

struct VulkanPipeline {
public:
    static constexpr u32 ATTRIBUTE_COUNT{ 2 };
    VkPipeline                   handle;
    VkPipelineLayout             pipeline_layout;
public:
    static bool create(VulkanContext& context);
    static FixedArray<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> get_attr_description();
    void destroy(const VulkanDevice& device);
};


} // sf
