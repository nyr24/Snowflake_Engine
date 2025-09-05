#include "sf_containers/optional.hpp"
#include "sf_core/application.hpp"
#include "sf_core/event.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_vulkan/synch.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_platform/platform.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_vulkan/frame.hpp"
// #include "sf_vulkan/allocator.hpp"
// #include "sf_allocators/free_list_allocator.hpp"
// #include <list>
#include <span>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

namespace sf {

static VulkanRenderer vk_renderer{};
static VulkanContext vk_context{};

static constexpr u8 REQUIRED_VALIDATION_LAYER_CAPACITY{ 1 };

bool create_instance(ApplicationConfig& config, PlatformState& platform_state);
void create_debugger();
bool init_extensions(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY>& required_extensions);
bool init_validation_layers(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_VALIDATION_LAYER_CAPACITY>& required_validation_layers);
void init_synch_primitives(VulkanContext& context);
void destroy_synch_primitives(VulkanContext& context);

bool renderer_init(ApplicationConfig& config, PlatformState& platform_state) {
    if (!create_instance(config, platform_state)) {
        return false;
    }

    platform_create_vk_surface(platform_state, vk_context);

    if (!VulkanDevice::create(vk_context)) {
        return false;
    }

    if (!VulkanSwapchain::create(vk_context, vk_context.framebuffer_width, vk_context.framebuffer_height, vk_context.swapchain)) {
        return false;
    }

    if (!VulkanPipeline::create(vk_context)) {
        return false;
    }

    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::GRAPHICS, vk_context.device.queue_family_info.graphics_family_index,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, vk_context.graphics_command_pool);
    VulkanCommandPool::create(vk_context, VulkanCommandPoolType::TRANSFER, vk_context.device.queue_family_info.transfer_family_index,
        static_cast<VkCommandPoolCreateFlagBits>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT), vk_context.transfer_command_pool);

    if (!VulkanCoherentBuffer::create(vk_context.device, define_vertices(), define_indices(), vk_context.coherent_buffer)) {
       return false; 
    }

    VulkanCommandBuffer::allocate(vk_context, vk_context.graphics_command_pool.handle, std::span{vk_context.graphics_command_buffers.data(), vk_context.graphics_command_buffers.capacity()}, true);
    VulkanCommandBuffer::allocate(vk_context, vk_context.transfer_command_pool.handle, std::span{vk_context.transfer_command_buffers.data(), vk_context.transfer_command_buffers.capacity()}, true);

    init_synch_primitives(vk_context);

    // set events
    event_set_listener(SystemEventCode::RESIZED, nullptr, renderer_on_resize);

    return true;
}

bool renderer_on_resize(u8 code, void* sender, void* listener_inst, EventContext* context) {
    if (code != SystemEventCode::RESIZED || !context) {
        return false;
    }

    vk_context.framebuffer_width = static_cast<u16>(context->data.u16[0]);
    vk_context.framebuffer_height = static_cast<u16>(context->data.u16[1]);
    vk_context.framebuffer_size_generation++;

    return true;
}

bool renderer_begin_frame(f64 delta_time) {
    vkQueueWaitIdle(vk_context.device.present_queue);

    if (vk_context.recreating_swapchain) {
        return false;
    }

    if (vk_context.framebuffer_last_size_generation != vk_context.framebuffer_size_generation) {
        vk_context.recreating_swapchain = true;
        vkDeviceWaitIdle(vk_context.device.logical_device);
        vk_context.swapchain.recreate(vk_context, vk_context.framebuffer_width, vk_context.framebuffer_height);
        vk_context.recreating_swapchain = false;
        vk_context.framebuffer_last_size_generation = vk_context.framebuffer_size_generation;
        return false;
    }
    
    if (!vk_context.swapchain.acquire_next_image_index(vk_context, UINT64_MAX, vk_context.image_available_semaphores[vk_context.curr_frame].handle, nullptr, vk_context.image_index)) {
        return false;
    }
    
    return true;
}

bool renderer_draw_frame(const RenderPacket& packet) {
    VulkanCommandBuffer& curr_graphics_buffer = vk_context.graphics_command_buffers[vk_context.curr_frame];
    VulkanCommandBuffer& curr_transfer_buffer = vk_context.transfer_command_buffers[vk_context.curr_frame];
    VulkanSemaphore& curr_image_available_semaphore = vk_context.image_available_semaphores[vk_context.curr_frame];
    VulkanSemaphore& curr_render_finished_semaphore = vk_context.render_finished_semaphores[vk_context.curr_frame];
    VulkanFence& curr_draw_fence = vk_context.draw_fences[vk_context.curr_frame];
    VulkanFence& curr_transfer_fence = vk_context.transfer_fences[vk_context.curr_frame];

    // THINK: we maybe need semaphore to synch copy and draw commands
    curr_transfer_buffer.begin_recording(vk_context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    u32 indices_byte_offset{ static_cast<u32>(vk_context.coherent_buffer.indeces_offset * sizeof(Vertex)) };

    VkBufferCopy vertex_copy_region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = indices_byte_offset 
    };

    VkBufferCopy index_copy_region{
        .srcOffset = indices_byte_offset,
        .dstOffset = indices_byte_offset,
        .size = vk_context.coherent_buffer.indeces_count * sizeof(u16)
    };
    
    curr_transfer_buffer.copy_buffer_data(vk_context.coherent_buffer.staging_buffer.handle, vk_context.coherent_buffer.main_buffer.handle, &vertex_copy_region);
    curr_transfer_buffer.copy_buffer_data(vk_context.coherent_buffer.staging_buffer.handle, vk_context.coherent_buffer.main_buffer.handle, &index_copy_region);
    curr_transfer_buffer.end_recording(vk_context);

    VkSubmitInfo transfer_submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &curr_transfer_buffer.handle  
    };

    curr_transfer_buffer.submit(vk_context, vk_context.device.transfer_queue, transfer_submit_info, {curr_transfer_fence});

    curr_graphics_buffer.begin_recording(vk_context, 0);
    curr_graphics_buffer.record_draw_commands(vk_context, vk_context.image_index);
    curr_graphics_buffer.end_recording(vk_context);
    
    VkPipelineStageFlags wait_dest_mask{ VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curr_image_available_semaphore.handle,
        .pWaitDstStageMask = &wait_dest_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &curr_graphics_buffer.handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curr_render_finished_semaphore.handle,
    };

    curr_graphics_buffer.submit(vk_context, vk_context.device.present_queue, submit_info, Option<VulkanFence>{curr_draw_fence});

    if (!curr_transfer_fence.wait(vk_context)) {
        return false;
    }
    curr_transfer_buffer.reset(vk_context);

    if (!curr_draw_fence.wait(vk_context)) {
        return false;
    }
    curr_graphics_buffer.reset(vk_context);

    vk_context.swapchain.present(vk_context, vk_context.device.present_queue, curr_render_finished_semaphore.handle, vk_context.image_index);
    return true;
}

void renderer_end_frame(f64 delta_time) {
    vk_context.draw_fences[vk_context.curr_frame].reset(vk_context);
    vk_context.transfer_fences[vk_context.curr_frame].reset(vk_context);
    ++vk_renderer.frame_count;
    vk_context.curr_frame = (vk_context.curr_frame + 1) % VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
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

VulkanContext::VulkanContext()
    : curr_frame{0}
{
    graphics_command_buffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    transfer_command_buffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    image_available_semaphores.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    draw_fences.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    transfer_fences.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
}

VulkanContext::~VulkanContext() {
    vkDeviceWaitIdle(vk_context.device.logical_device);

    destroy_synch_primitives(*this);
    
    if (transfer_command_pool.handle) {
        for (auto& cmd_buffer : transfer_command_buffers) {
            cmd_buffer.free(*this, transfer_command_pool.handle);
        }
    }
    if (graphics_command_pool.handle) {
        for (auto& cmd_buffer : graphics_command_buffers) {
            cmd_buffer.free(*this, graphics_command_pool.handle);
        }
    }

    coherent_buffer.destroy(vk_context.device);

    transfer_command_pool.destroy(*this);
    graphics_command_pool.destroy(*this);

    swapchain.destroy(*this);

    vk_context.device.destroy(*this);

    if (surface) {
        vkDestroySurfaceKHR(instance, surface, &allocator);
        surface = nullptr;
    }

    if (debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT debug_destroy_fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        debug_destroy_fn(instance, debug_messenger, &allocator);
    }

    if (instance) {
        vkDestroyInstance(instance, &allocator);
        instance = nullptr;
    }
}

bool create_instance(ApplicationConfig& config, PlatformState& platform_state) {
    vk_renderer.platform_state = &platform_state;
    vk_renderer.frame_count = 0;

    VkApplicationInfo vk_app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    vk_app_info.pApplicationName = config.name;
    vk_app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.pEngineName = "SNOWFLAKE_ENGINE";
    vk_app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vk_app_info.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pNext = nullptr;
    create_info.pApplicationInfo = &vk_app_info;

    // Extensions
    FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY> required_extensions;
    required_extensions.append(VK_KHR_SURFACE_EXTENSION_NAME);
    #ifdef SF_DEBUG
    required_extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    if (!init_extensions(create_info, required_extensions)) {
        return false;
    }

    // Layers
    #ifdef SF_DEBUG
    FixedArray<const char*, 1> required_validation_layers;
    required_validation_layers.append("VK_LAYER_KHRONOS_validation");

    if (!init_validation_layers(create_info, required_validation_layers)) {
        return false;
    }
    #endif

    // TODO: custom allocator
    sf_vk_check(vkCreateInstance(&create_info, nullptr, &vk_context.instance));

    #ifdef SF_DEBUG
    create_debugger();
    #endif

    vk_context.framebuffer_width = config.width;
    vk_context.framebuffer_height = config.height;

    return true;
}

void create_debugger() {
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

    // TODO: custom allocator
    sf_vk_check(func(vk_context.instance, &debug_create_info, nullptr, &vk_context.debug_messenger));
}

bool init_extensions(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY>& required_extensions) {
    platform_get_required_extensions(required_extensions);
#ifdef SF_DEBUG
    // Log required extensions
    LOG_INFO("Required vulkan extensions: ");
    for (const char* ext_name : required_extensions) {
        LOG_INFO("{}", ext_name);
    }
#endif
    u32 available_extensions_count{0};
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, nullptr);

    FixedArray<VkExtensionProperties, 40> available_extensions(available_extensions_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, available_extensions.data());

    // check if we have all required extensions
    for (const char* req_ext_name : required_extensions) {
        bool is_found{false};
        for (u32 i{0}; i < available_extensions_count; ++i) {
            const auto& av_ext = available_extensions[i];
            if (sf_str_cmp(req_ext_name, av_ext.extensionName)) {
                is_found = true;
                break;
            }
        }
        if (!is_found) {
            LOG_FATAL("Missing required extension: {}", req_ext_name);
            return false;
        }
    }

    create_info.enabledExtensionCount = required_extensions.count();
    create_info.ppEnabledExtensionNames = required_extensions.data();

    return true;
}

bool init_validation_layers(VkInstanceCreateInfo& create_info, FixedArray<const char*, REQUIRED_VALIDATION_LAYER_CAPACITY>& required_validation_layers) {
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

    create_info.enabledLayerCount = required_validation_layers.count();
    create_info.ppEnabledLayerNames = required_validation_layers.data();
    return true;
}

void init_synch_primitives(VulkanContext& context) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        context.image_available_semaphores[i] = VulkanSemaphore::create(context);
        context.render_finished_semaphores[i] = VulkanSemaphore::create(context);
        context.draw_fences[i] = VulkanFence::create(context, false);
        context.transfer_fences[i] = VulkanFence::create(context, false);
    }
}

void destroy_synch_primitives(VulkanContext& context) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        context.image_available_semaphores[i].destroy(context);
        context.render_finished_semaphores[i].destroy(context);
        context.draw_fences[i].destroy(context);
        context.transfer_fences[i].destroy(context);
    }
}

VkViewport VulkanContext::get_viewport() const {
    return {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(framebuffer_width),
        .height = static_cast<float>(framebuffer_height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
}

VkRect2D VulkanContext::get_scissors() const {
    return VkRect2D{
        .offset = { 0, 0 },
        .extent = { framebuffer_width, framebuffer_height }
    };
}

} // sf
