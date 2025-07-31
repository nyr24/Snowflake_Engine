#include "sf_vulkan/device.hpp"
#include "sf_core/logger.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool device_create(VulkanContext& context) {
    return device_select(context);
}

bool device_destroy(VulkanContext& context);

bool device_select(VulkanContext& context) {
    u32 physical_device_count{0};
    vkEnumeratePhysicalDevices(context.instance, &physical_device_count, nullptr);

    VkPhysicalDevice physical_devices[10];
    vkEnumeratePhysicalDevices(context.instance, &physical_device_count, physical_devices);

    for (u32 i{0}; i < physical_device_count; ++i) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties);

        LOG_INFO("Properties of device:\nname: {}, type: {}", static_cast<const char*>(physical_device_properties.deviceName), static_cast<i32>(physical_device_properties.deviceType));

        VkPhysicalDeviceFeatures physical_device_features;
        vkGetPhysicalDeviceFeatures(physical_devices[i], &physical_device_features);

        VkPhysicalDeviceMemoryProperties physical_device_memory_props;
        vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &physical_device_memory_props);
    }

    return true;
}

} // sf
