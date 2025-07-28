#include "sf_renderer/renderer.hpp"
#include "sf_core/logger.hpp"
#include "sf_platform/platform.hpp"
#include "sf_ds/array_list.hpp"
#include "sf_core/memory_sf.hpp"
#include <span>
#include <vulkan/vulkan_core.h>

namespace sf {
static VulkanRenderer vk_renderer{};
static VulkanContext vk_context{};

bool renderer_init(const char* app_name, PlatformState* platform_state) {
    vk_renderer.platform_state = platform_state;
    vk_renderer.frame_count = 0;

    VkApplicationInfo vk_app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    vk_app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.apiVersion = VK_API_VERSION_1_4;
    vk_app_info.pApplicationName = app_name;

    VkInstanceCreateInfo vk_inst_create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vk_inst_create_info.pApplicationInfo = &vk_app_info;

    FixedArrayList<const char*, 3> required_extensions;
    required_extensions.append(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef SF_DEBUG
    required_extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Log required extensions
    LOG_INFO("Required vulkan extensions: ");
    for (const char* ext_name : required_extensions) {
        LOG_INFO("{}", ext_name);
    }
#endif
    platform_get_required_extension_names(std::span{ required_extensions.last(), required_extensions.capacity_remain() });

    vk_inst_create_info.enabledExtensionCount = required_extensions.count();
    vk_inst_create_info.ppEnabledExtensionNames = required_extensions.data();

    // Validation layers should be only enabled in debug mode
#ifdef SF_DEBUG
    sf::FixedArrayList<const char*, 1> required_validation_layers;
    required_validation_layers.append("VK_LAYER_KHRONOS_validation");

    u32 available_layer_count = 0;
    sf_vk_check(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    ArrayList<VkLayerProperties> available_layers(available_layer_count, available_layer_count);
    sf_vk_check(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data()));

    LOG_INFO("Checking validation layers availability...");
    for (const char* req_layer : required_validation_layers) {
        bool is_available = false;
        for (const VkLayerProperties& available_layer : available_layers) {
            if (sf_str_cmp(req_layer, available_layer.layerName) == 0) {
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

    VkResult result = vkCreateInstance(&vk_inst_create_info, nullptr, &vk_context.instance);
    if (result != VK_SUCCESS) {
        return false;
    }

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

} // sf
