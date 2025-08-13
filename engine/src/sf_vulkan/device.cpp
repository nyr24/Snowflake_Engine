#include "sf_vulkan/device.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_vulkan/types.hpp"
#include "sf_containers/fixed_array.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool device_create(VulkanContext& context) {
    if (!device_select(context)) {
        return false;
    }

    // Do not create additional queues for shared indices.
    bool present_shares_graphics_queue = context.device.queue_family_info.graphics_family_index == context.device.queue_family_info.present_family_index;
    bool transfer_shares_other_queues = context.device.queue_family_info.transfer_family_index == context.device.queue_family_info.graphics_family_index
        || context.device.queue_family_info.transfer_family_index == context.device.queue_family_info.present_family_index;

    u32 index_count = 1;
    if (!present_shares_graphics_queue) {
        index_count++;
    }
    if (!transfer_shares_other_queues) {
        index_count++;
    }

    constexpr u32 MAX_INDEX_COUNT = 3;
    FixedArray<u32, MAX_INDEX_COUNT> indices(index_count);

    u8 index = 0;
    indices[index++] = context.device.queue_family_info.graphics_family_index;
    if (!present_shares_graphics_queue) {
        indices[index++] = context.device.queue_family_info.present_family_index;
    }
    if (!transfer_shares_other_queues) {
        indices[index++] = context.device.queue_family_info.transfer_family_index;
    }

    FixedArray<VkDeviceQueueCreateInfo, MAX_INDEX_COUNT> queue_create_infos(index_count);

    for (u32 i = 0; i < index_count; ++i) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = indices[i];
        queue_create_infos[i].queueCount = 1;
        if (indices[i] == context.device.queue_family_info.graphics_family_index && context.device.queue_family_info.graphics_available_queue_count >= 2) {
            queue_create_infos[i].queueCount = 2;
        }
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].pNext = 0;
        f32 queue_priority = 1.0f;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }

    // Request device features.
    // TODO: should be config driven
    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;  // Request anistrophy

    VkDeviceCreateInfo device_create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_create_info.queueCreateInfoCount = index_count;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 1;
    const char* extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    device_create_info.ppEnabledExtensionNames = &extension_names;

    // Create the device.
    sf_vk_check(vkCreateDevice(
        context.device.physical_device,
        &device_create_info,
        // &context.allocator.callbacks,
        nullptr,
        &context.device.logical_device)
    );

     // Get queues.
    vkGetDeviceQueue(
        context.device.logical_device,
        context.device.queue_family_info.graphics_family_index,
        0,
        &context.device.graphics_queue);

    vkGetDeviceQueue(
        context.device.logical_device,
        context.device.queue_family_info.present_family_index,
        0,
        &context.device.present_queue);

    vkGetDeviceQueue(
        context.device.logical_device,
        context.device.queue_family_info.transfer_family_index,
        0,
        &context.device.transfer_queue);

    LOG_INFO("Logical device and queues were successfully created");

    return true;
}

bool device_select(VulkanContext& context) {
    u32 physical_device_count{0};
    sf_vk_check(vkEnumeratePhysicalDevices(context.instance, &physical_device_count, nullptr));

    const u32 physical_device_max_count{5};
    VkPhysicalDevice physical_devices[physical_device_max_count];
    sf_vk_check(vkEnumeratePhysicalDevices(context.instance, &physical_device_count, physical_devices));

    SF_ASSERT_MSG(physical_device_max_count >= physical_device_count, "Enumerated physical devices wouldn't feet into array");

    VulkanPhysicalDeviceRequirements physical_device_requirements{
        .flags = VULKAN_PHYSICAL_DEVICE_REQUIREMENT_GRAPHICS
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_PRESENT
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_TRANSFER
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_SAMPLER_ANISOTROPY
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_DISCRETE_GPU
            // include this if need compute
            // | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_COMPUTE
        ,
        .device_extension_names = Option<FixedArray<const char*, 10>>(FixedArray<const char*, 10>{ (const char*)(VK_KHR_SWAPCHAIN_EXTENSION_NAME) })
    };

    for (u32 i{0}; i < physical_device_count; ++i) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties);

        VkPhysicalDeviceFeatures physical_device_features;
        vkGetPhysicalDeviceFeatures(physical_devices[i], &physical_device_features);

        VulkanDeviceQueueFamilyInfo queue_family_info;
        VulkanSwapchainSupportInfo swapchain_support_info;
        bool meets_requirements = device_meet_requirements(physical_devices[i], context.surface, physical_device_properties, physical_device_features, physical_device_requirements, queue_family_info, swapchain_support_info);

        if (meets_requirements) {
        #ifdef SF_DEBUG
            LOG_INFO("Selected device: {}", physical_device_properties.deviceName);

            switch (physical_device_properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                LOG_INFO("GPU type is: Discrete");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                LOG_INFO("GPU type is: Integrated");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                LOG_INFO("GPU type is: CPU");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                LOG_INFO("GPU type is: Virtual");
                break;
            default:
                LOG_INFO("GPU type is: Unknown");
                break;
            }

            LOG_INFO(
                "GPU Driver version: {}.{}.{}",
                VK_VERSION_MAJOR(physical_device_properties.driverVersion),
                VK_VERSION_MINOR(physical_device_properties.driverVersion),
                VK_VERSION_PATCH(physical_device_properties.driverVersion));

            // Vulkan API version.
            LOG_INFO(
                "Vulkan API version: {}.{}.{}",
                VK_VERSION_MAJOR(physical_device_properties.apiVersion),
                VK_VERSION_MINOR(physical_device_properties.apiVersion),
                VK_VERSION_PATCH(physical_device_properties.apiVersion)
            );


            VkPhysicalDeviceMemoryProperties physical_device_memory_props;
            vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &physical_device_memory_props);

            // Memory information
            for (u32 j = 0; j < physical_device_memory_props.memoryHeapCount; ++j) {
                f32 memory_size_gib = ((static_cast<f32>(physical_device_memory_props.memoryHeaps[j].size)) / 1024.0f / 1024.0f / 1024.0f);
                if (physical_device_memory_props.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    LOG_INFO("Local GPU memory: {} GiB", memory_size_gib);
                } else {
                    LOG_INFO("Shared System memory: {} GiB", memory_size_gib);
                }
            }
        #endif // SF_DEBUG

            context.device.physical_device = physical_devices[i];
            context.device.queue_family_info = queue_family_info;
            context.device.properties = physical_device_properties;
            context.device.properties = physical_device_properties;
            context.device.properties = physical_device_properties;
            context.device.features = physical_device_features;
            context.device.memory_properties = physical_device_memory_props;

            return true;
        }
    }

    return false;
}

void device_query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo& out_support_info) {
    sf_vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &out_support_info.capabilities));
    sf_vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out_support_info.format_count, nullptr));

    if (out_support_info.format_count) {
        if (out_support_info.formats.count() < out_support_info.format_count) {
            out_support_info.formats.reallocate_and_resize(out_support_info.format_count, out_support_info.format_count);
        }

        sf_vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out_support_info.format_count, out_support_info.formats.data()));
    }


    sf_vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out_support_info.present_mode_count, nullptr));
    if (out_support_info.present_mode_count) {
        if (out_support_info.present_modes.count() < out_support_info.present_mode_count) {
            out_support_info.present_modes.reallocate_and_resize(out_support_info.present_mode_count, out_support_info.present_mode_count);
        }

        sf_vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out_support_info.present_mode_count, out_support_info.present_modes.data()));
    }
}

bool device_meet_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties& properties,
    const VkPhysicalDeviceFeatures& features,
    const VulkanPhysicalDeviceRequirements& requirements,
    VulkanDeviceQueueFamilyInfo& out_queue_family_info,
    VulkanSwapchainSupportInfo& out_swapchain_support_info
) {
    if (requirements.flags & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_DISCRETE_GPU) {
        if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            LOG_INFO("Device is not discrete gpu, skipping");
            return false;
        }
    }

    out_queue_family_info.compute_family_index = 255;
    out_queue_family_info.graphics_family_index = 255;
    out_queue_family_info.transfer_family_index = 255;
    out_queue_family_info.present_family_index = 255;

    u32 queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    FixedArray<VkQueueFamilyProperties, 15> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    u8 min_transfer_score{255};

    for (u32 i{0}; i < queue_family_count; ++i) {
        u8 curr_transfer_score{0};

        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            ++curr_transfer_score;
            out_queue_family_info.graphics_family_index = i;
            out_queue_family_info.graphics_available_queue_count = queue_families[i].queueCount;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            ++curr_transfer_score;
            out_queue_family_info.compute_family_index = i;
            out_queue_family_info.compute_available_queue_count = queue_families[i].queueCount;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (curr_transfer_score < min_transfer_score) {
                min_transfer_score = curr_transfer_score;
                out_queue_family_info.transfer_family_index = i;
                out_queue_family_info.transfer_available_queue_count = queue_families[i].queueCount;
            }
        }

        VkBool32 supports_present{VK_FALSE};
        sf_vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
        if (supports_present) {
            out_queue_family_info.present_family_index = i;
        }
    }

    LOG_INFO("Device = {}, queue info:\n\tGraphics queue index: {}\n\tCompute queue index: {}\n\tTransfer queue index: {}",
        properties.deviceName,
        out_queue_family_info.graphics_family_index,
        out_queue_family_info.compute_family_index,
        out_queue_family_info.transfer_family_index
    );

    bool device_meets_requirements = (!requirements_has_graphics(requirements.flags) || requirements_has_graphics(requirements.flags) && out_queue_family_info.graphics_family_index != 255)
        && (!requirements_has_compute(requirements.flags) || requirements_has_compute(requirements.flags) && out_queue_family_info.compute_family_index != 255)
        && (!requirements_has_transfer(requirements.flags) || requirements_has_transfer(requirements.flags) && out_queue_family_info.transfer_family_index != 255)
        && (!requirements_has_present(requirements.flags) || requirements_has_present(requirements.flags) && out_queue_family_info.present_family_index != 255);

    if (!device_meets_requirements) {
        return false;
    }

    device_query_swapchain_support(device, surface, out_swapchain_support_info);

    if (!out_swapchain_support_info.present_mode_count) {
        return false;
    }

    if (requirements.device_extension_names.is_some()) {
        u32 available_extension_count{0};
        sf_vk_check(vkEnumerateDeviceExtensionProperties(device, nullptr, &available_extension_count, nullptr));

        if (available_extension_count) {
            DynamicArray<VkExtensionProperties> available_extensions(available_extension_count, available_extension_count);
            sf_vk_check(vkEnumerateDeviceExtensionProperties(device, nullptr, &available_extension_count, available_extensions.data()));

            for (const auto* required_extension : requirements.device_extension_names.unwrap()) {
                LOG_INFO("Checking for device {}, extension: {}", properties.deviceName, required_extension);
                bool is_available{false};

                for (const auto& available_ext : available_extensions) {
                    if (sf_str_cmp(available_ext.extensionName, required_extension)) {
                        LOG_INFO("Extension: {} is found", required_extension);
                        is_available = true;
                        break;
                    }
                }

                if (!is_available) {
                    LOG_INFO("Required extension for device {} is not found: {}\nskipping device", properties.deviceName, required_extension);
                    return false;
                }
            }
        }
    }

    if (requirements_has_sampler_anisotropy(requirements.flags) && !features.samplerAnisotropy) {
        LOG_INFO("Device {} don't have sampler anisotropy, skipping", properties.deviceName);
        return false;
    }

    LOG_INFO("Device {} meets all requirements!", properties.deviceName);
    return true;
}

// called from VulkanContext destructor
void device_destroy(VulkanContext& context) {
    context.device.graphics_queue = nullptr;
    context.device.present_queue = nullptr;
    context.device.transfer_queue = nullptr;

    if (context.device.logical_device) {
        vkDestroyDevice(context.device.logical_device, nullptr);
        context.device.logical_device = nullptr;
    }

    context.device.physical_device = nullptr;

    context.device.queue_family_info.graphics_family_index = 255;
    context.device.queue_family_info.present_family_index = 255;
    context.device.queue_family_info.transfer_family_index = 255;
    context.device.queue_family_info.compute_family_index = 255;
}

bool requirements_has_graphics(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_GRAPHICS; }
bool requirements_has_present(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_PRESENT; }
bool requirements_has_compute(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_COMPUTE; }
bool requirements_has_transfer(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_TRANSFER; }
bool requirements_has_sampler_anisotropy(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_SAMPLER_ANISOTROPY; }
bool requirements_has_discrete(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_DISCRETE_GPU; }

} // sf
