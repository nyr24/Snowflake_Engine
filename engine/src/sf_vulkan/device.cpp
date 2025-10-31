#include "sf_vulkan/device.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_core/application.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_containers/fixed_array.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool VulkanDevice::create(VulkanContext& context) {
    FixedArray<const char*, DEVICE_EXTENSION_CAPACITY> extension_names{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

    if (!VulkanDevice::select(context, extension_names)) {
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
        indices[index++] = context.device.queue_family_info.present_family_index; }
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
    VkPhysicalDeviceFeatures device_features = { .samplerAnisotropy = VK_TRUE };

    VkPhysicalDeviceSynchronization2FeaturesKHR synch_2_feature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
        .synchronization2 = VK_TRUE,
    };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = &synch_2_feature,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamic_rendering_feature,
        .queueCreateInfoCount = index_count,
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = extension_names.count(),
        .ppEnabledExtensionNames = extension_names.data(),
        .pEnabledFeatures = &device_features,
    };

    // Create the device.
    sf_vk_check(vkCreateDevice(
        context.device.physical_device,
        &device_create_info,
        // TODO: custom allocator
        // &context.allocator,
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

bool VulkanDevice::select(VulkanContext& context, FixedArray<const char*, DEVICE_EXTENSION_CAPACITY>& required_extensions) {
    u32 physical_device_count{0};
    sf_vk_check(vkEnumeratePhysicalDevices(context.instance, &physical_device_count, nullptr));

    constexpr u32 PHYSICAL_DEVICE_MAX_COUNT{5};
    FixedArray<VkPhysicalDevice, PHYSICAL_DEVICE_MAX_COUNT> physical_devices(PHYSICAL_DEVICE_MAX_COUNT);
    sf_vk_check(vkEnumeratePhysicalDevices(context.instance, &physical_device_count, physical_devices.data()));

    SF_ASSERT_MSG(PHYSICAL_DEVICE_MAX_COUNT >= physical_device_count, "Enumerated physical devices wouldn't fit into array");

    VulkanPhysicalDeviceRequirements physical_device_requirements{
        .flags = VULKAN_PHYSICAL_DEVICE_REQUIREMENT_GRAPHICS
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_PRESENT
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_TRANSFER
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_SAMPLER_ANISOTROPY
            | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_DISCRETE_GPU
            // include this if need compute
            // | VULKAN_PHYSICAL_DEVICE_REQUIREMENT_COMPUTE
        ,
        .device_extension_names = required_extensions
    };

    for (u32 i{0}; i < physical_device_count; ++i) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties);

        VkPhysicalDeviceFeatures physical_device_features;
        vkGetPhysicalDeviceFeatures(physical_devices[i], &physical_device_features);

        VulkanDeviceQueueFamilyInfo queue_family_info;

        VulkanSwapchainSupportInfo swapchain_support_info;
        swapchain_support_info.set_allocator(context.render_system_allocator);

        bool meets_requirements = VulkanDevice::meet_requirements(
            physical_devices[i], context.surface, physical_device_properties,
            physical_device_features, physical_device_requirements, queue_family_info,
            swapchain_support_info, context.render_system_allocator
        );

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

            LOG_INFO(
                "Vulkan API version: {}.{}.{}",
                VK_VERSION_MAJOR(physical_device_properties.apiVersion),
                VK_VERSION_MINOR(physical_device_properties.apiVersion),
                VK_VERSION_PATCH(physical_device_properties.apiVersion)
            );

        #endif
            VkPhysicalDeviceMemoryProperties physical_device_memory_props;
            vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &physical_device_memory_props);

        #ifdef SF_DEBUG
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
            context.device.features = physical_device_features;
            context.device.memory_properties = physical_device_memory_props;
            context.device.swapchain_support_info = std::move(swapchain_support_info);

            return true;
        }
    }

    return false;
}

void VulkanDevice::query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo& out_support_info) {
    sf_vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &out_support_info.capabilities));
    sf_vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out_support_info.format_count, nullptr));

    if (out_support_info.format_count) {
        if (out_support_info.formats.count() < out_support_info.format_count) {
            out_support_info.formats.reserve_and_resize(out_support_info.format_count, out_support_info.format_count);
        }

        sf_vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out_support_info.format_count, out_support_info.formats.data()));
    }


    sf_vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out_support_info.present_mode_count, nullptr));
    if (out_support_info.present_mode_count) {
        if (out_support_info.present_modes.count() < out_support_info.present_mode_count) {
            out_support_info.present_modes.reserve_and_resize(out_support_info.present_mode_count, out_support_info.present_mode_count);
        }

        sf_vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out_support_info.present_mode_count, out_support_info.present_modes.data()));
    }
}

bool VulkanDevice::meet_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties& properties,
    const VkPhysicalDeviceFeatures& features,
    const VulkanPhysicalDeviceRequirements& requirements,
    VulkanDeviceQueueFamilyInfo& out_queue_family_info,
    VulkanSwapchainSupportInfo& out_swapchain_support_info,
    LinearAllocator& render_system_allocator
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

        if (out_queue_family_info.present_family_index != 255) {
            continue;
        }

        VkBool32 supports_present{VK_FALSE};
        sf_vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
        if (supports_present) {
            out_queue_family_info.present_family_index = i;
        }
    }

    LOG_INFO("Device = {}, queue info:\n\tGraphics queue index: {}\n\tPresent queue index: {}\n\tCompute queue index: {}\n\tTransfer queue index: {}",
        properties.deviceName,
        out_queue_family_info.graphics_family_index,
        out_queue_family_info.present_family_index,
        out_queue_family_info.compute_family_index,
        out_queue_family_info.transfer_family_index
    );

    bool device_meets_requirements = (!requirements_has_graphics(requirements.flags) || (requirements_has_graphics(requirements.flags) && out_queue_family_info.graphics_family_index != 255))
        && (!requirements_has_compute(requirements.flags) || (requirements_has_compute(requirements.flags) && out_queue_family_info.compute_family_index != 255))
        && (!requirements_has_transfer(requirements.flags) || (requirements_has_transfer(requirements.flags) && out_queue_family_info.transfer_family_index != 255))
        && (!requirements_has_present(requirements.flags) || (requirements_has_present(requirements.flags) && out_queue_family_info.present_family_index != 255));

    if (!device_meets_requirements) {
        return false;
    }

    VulkanDevice::query_swapchain_support(device, surface, out_swapchain_support_info);

    if (!out_swapchain_support_info.present_mode_count) {
        return false;
    }

    if (requirements.device_extension_names.count() > 0) {
        u32 available_extension_count{0};
        sf_vk_check(vkEnumerateDeviceExtensionProperties(device, nullptr, &available_extension_count, nullptr));

        if (available_extension_count) {
            DynamicArray<VkExtensionProperties, StackAllocator> available_extensions(available_extension_count, available_extension_count, &application_get_temp_allocator());
            sf_vk_check(vkEnumerateDeviceExtensionProperties(device, nullptr, &available_extension_count, available_extensions.data()));

            for (const auto* required_extension : requirements.device_extension_names) {
                bool is_available{false};

                for (const auto& available_ext : available_extensions) {
                    if (sf_str_cmp(available_ext.extensionName, required_extension)) {
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

void VulkanSwapchainSupportInfo::set_allocator(LinearAllocator& allocator)
{
    formats.set_allocator(&allocator);
    present_modes.set_allocator(&allocator);
}

bool VulkanDevice::detect_depth_format() {
    constexpr u8 CANDIDATES_COUNT{3};
    VkFormat candidates[CANDIDATES_COUNT] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    u32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (u32 i{0}; i < CANDIDATES_COUNT; ++i) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physical_device, candidates[i], &properties);
        if ((properties.linearTilingFeatures & flags) == flags) {
            depth_format = candidates[i];
            return true;
        } else if ((properties.optimalTilingFeatures & flags) == flags) {
            depth_format = candidates[i];
            return true;
        }
    }

    return false;
}

// called from VulkanContext destructor
void VulkanDevice::destroy(VulkanContext& context) {
    graphics_queue = nullptr;
    present_queue = nullptr;
    transfer_queue = nullptr;

    if (logical_device) {
        // TODO: custom allocator
        // vkDestroyDevice(logical_device, &context.allocator);
        vkDestroyDevice(logical_device, nullptr);
        logical_device = nullptr;
    }

    physical_device = nullptr;

    queue_family_info.graphics_family_index = 255;
    queue_family_info.present_family_index = 255;
    queue_family_info.transfer_family_index = 255;
    queue_family_info.compute_family_index = 255;
}

Option<u32> VulkanDevice::find_memory_index(u32 type_filter, VkMemoryPropertyFlagBits property_flags) const {
    for (u32 i{0}; i < memory_properties.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)) {
            return {i};
        }
    }

    return {None::VALUE};
}

bool requirements_has_graphics(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_GRAPHICS; }
bool requirements_has_present(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_PRESENT; }
bool requirements_has_compute(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_COMPUTE; }
bool requirements_has_transfer(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_TRANSFER; }
bool requirements_has_sampler_anisotropy(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_SAMPLER_ANISOTROPY; }
bool requirements_has_discrete(VulkanPhysicalDeviceRequirementsFlags requirements) { return requirements & VULKAN_PHYSICAL_DEVICE_REQUIREMENT_DISCRETE_GPU; }

} // sf
