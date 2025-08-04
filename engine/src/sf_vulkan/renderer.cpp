#include "sf_vulkan/types.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_ds/allocator.hpp"
#include "sf_vulkan/allocator.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/types.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_platform/platform.hpp"
#include "sf_ds/fixed_array.hpp"
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

namespace sf {
static VulkanRenderer vk_renderer{};

static VulkanContext vk_context{
    .allocator = VulkanAllocator{
        .state = VulkanAllocatorState(platform_get_mem_page_size() * 10),
        .callbacks = VkAllocationCallbacks{
            .pfnAllocation = vk_alloc_fn,
            .pfnReallocation = vk_realloc_fn,
            .pfnFree = vk_free_fn,
            .pfnInternalAllocation = vk_intern_alloc_notification,
            .pfnInternalFree = vk_intern_free_notification
        }
    }
};

bool renderer_init(const char* app_name, PlatformState& platform_state) {
    vk_renderer.platform_state = &platform_state;
    vk_renderer.frame_count = 0;

    VkApplicationInfo vk_app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    vk_app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.apiVersion = VK_API_VERSION_1_4;
    vk_app_info.pApplicationName = app_name;

    VkInstanceCreateInfo vk_inst_create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vk_inst_create_info.pApplicationInfo = &vk_app_info;

    FixedArray<const char*, 5> required_extensions;
    required_extensions.append(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef SF_DEBUG
    required_extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    platform_get_required_extensions(required_extensions);
#ifdef SF_DEBUG
    // Log required extensions
    LOG_INFO("Required vulkan extensions: ");
    for (const char* ext_name : required_extensions) {
        LOG_INFO("{}", ext_name);
    }

    u32 available_extensions_count{0};
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, nullptr);

    FixedArray<VkExtensionProperties, 100> available_extensions(available_extensions_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, available_extensions.data());

    LOG_INFO("Available vulkan extensions: ");
    for (const VkExtensionProperties& available_extension : available_extensions) {
        LOG_INFO("{}", available_extension.extensionName);
    }
#endif

    vk_inst_create_info.enabledExtensionCount = required_extensions.count();
    vk_inst_create_info.ppEnabledExtensionNames = required_extensions.data();

    // Validation layers should only be enabled in debug mode
#ifdef SF_DEBUG
    FixedArray<const char*, 1> required_validation_layers;
    required_validation_layers.append("VK_LAYER_KHRONOS_validation");

    u32 available_layer_count = 0;
    sf_vk_check(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    FixedArray<VkLayerProperties, 30> available_layers(available_layer_count);
    sf_vk_check(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data()));

    for (const char* req_layer : required_validation_layers) {
        bool is_available = false;
        for (const VkLayerProperties& available_layer : available_layers) {
            if (sf_str_cmp(req_layer, available_layer.layerName)) {
                is_available = true;
                break;
            }
        }

        if (!is_available) {
            LOG_FATAL("Required validation layer is missing: {}", req_layer);
            return false;
        }
    }

    vk_inst_create_info.enabledLayerCount = required_validation_layers.count();
    vk_inst_create_info.ppEnabledLayerNames = required_validation_layers.data();
#endif

    sf_vk_check(vkCreateInstance(&vk_inst_create_info, nullptr, &vk_context.instance));

    // Debugger
#ifdef SF_DEBUG
    u32 log_severity =  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
                        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = sf_vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_context.instance, "vkCreateDebugUtilsMessengerEXT");

    SF_ASSERT_MSG(func, "Failed to create debug messenger!");

    sf_vk_check(func(vk_context.instance, &debug_create_info, nullptr, &vk_context.debug_messenger));
#endif

    platform_create_vk_surface(platform_state, vk_context);

    device_create(vk_context);

    return true;
}

void renderer_resize(i16 width, i16 height) {
}

bool renderer_begin_frame(f64 delta_time) {
    return true;
}

bool renderer_end_frame(f64 delta_time) {
    ++vk_renderer.frame_count;
    return true;
}

bool renderer_draw_frame(const RenderPacket& packet) {
    if (renderer_begin_frame(packet.delta_time)) {

        // TODO:

        bool end_frame_res = renderer_end_frame(packet.delta_time);

        if (!end_frame_res) {
            LOG_ERROR("end_frame failed, application shutting down...");
            return false;
        }
    }

    return true;
}

void sf_vk_check(VkResult vk_result) {
    if (vk_result != VK_SUCCESS) {
        panic("Vk check failed!");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL sf_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           message_severity,
    VkDebugUtilsMessageTypeFlagsEXT                  message_types,
    const VkDebugUtilsMessengerCallbackDataEXT*      callback_data,
    void*                                            user_data
) {
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            LOG_ERROR("{}", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            LOG_WARN("{}", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            LOG_INFO("{}", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            LOG_TRACE("{}", callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

VulkanContext::~VulkanContext() {
    if (debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT debug_destroy_fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        debug_destroy_fn(instance, debug_messenger, &allocator.callbacks);
    }

    vkDestroyInstance(instance, &allocator.callbacks);
}

} // sf
