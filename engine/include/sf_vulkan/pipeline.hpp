#pragma once

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
    VkPipeline          handle;
    VkPipelineLayout    layout;
public:
    static bool create(VulkanContext& context);
};


} // sf
