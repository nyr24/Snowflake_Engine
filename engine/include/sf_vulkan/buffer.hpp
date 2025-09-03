#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/pipeline.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanBuffer {
public:
    enum struct Type {
        VERTEX,
        UNIFORM,
        INDEX,
    };
    
public:
    DynamicArray<Vertex>    geometry;
    Type                    type;
    VkBuffer                handle;
    VkDeviceMemory          memory;
public:
    static bool create(const VulkanDevice& device, DynamicArray<Vertex>&& geometry, Type type, VulkanBuffer& out_buffer);
    static VkBufferUsageFlagBits map_type_to_usage_flag(Type type);
    void destroy(const VulkanDevice& device);
    bool copy_geometry_to_gpu(const VulkanDevice& device);
};

} // sf
