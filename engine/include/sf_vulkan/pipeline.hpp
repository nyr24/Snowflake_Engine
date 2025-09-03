#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace sf {

struct VulkanContext;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription get_binding_descr();
};

struct VulkanPipeline {
public:
    static constexpr u32 ATTRIBUTE_COUNT{ 2 };
    VkPipeline          handle;
    VkPipelineLayout    layout;
public:
    static bool create(VulkanContext& context);
    static DynamicArray<Vertex> define_geometry();
    static FixedArray<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> get_attr_description();
};


} // sf
